// Package dns configures system DNS resolution for the Coder VPN tunnel.
//
// It implements a three-backend cascade:
//  1. resolvconf — if the binary is available
//  2. resolvectl — if the binary is available (systemd-resolved)
//  3. direct file — modify /etc/resolv.conf directly as a last resort
//
// This mirrors the logic in app/src/vpn/DnsManager.cpp.
package dns

import (
	"fmt"
	"log"
	"net/netip"
	"os"
	"os/exec"
	"strings"

	tsdns "tailscale.com/net/dns"
)

// backend represents a DNS configuration strategy.
type backend int

const (
	backendResolvconf backend = iota
	backendResolvectl
	backendDirectFile
)

// Manager configures and tears down DNS for the VPN TUN interface.
type Manager struct {
	ifName  string
	backend backend

	// directFile state
	backupPath string // set when we've backed up /etc/resolv.conf
}

// NewManager creates a DNS manager for the given TUN interface name.
// The backend is auto-detected based on which tools are available.
func NewManager(ifName string) *Manager {
	m := &Manager{ifName: ifName}

	if _, err := exec.LookPath("resolvconf"); err == nil {
		m.backend = backendResolvconf
		log.Printf("dns: using resolvconf backend")
	} else if _, err := exec.LookPath("resolvectl"); err == nil {
		m.backend = backendResolvectl
		log.Printf("dns: using resolvectl backend")
	} else {
		m.backend = backendDirectFile
		log.Printf("dns: using direct /etc/resolv.conf backend")
	}

	return m
}

// Configure applies the given DNS servers and search domains for the TUN
// interface. nameservers are IP strings, domains are plain domain names.
func (m *Manager) Configure(nameservers, searchDomains []string) error {
	switch m.backend {
	case backendResolvconf:
		return m.configureResolvconf(nameservers, searchDomains)
	case backendResolvectl:
		return m.configureResolvectl(nameservers, searchDomains)
	case backendDirectFile:
		return m.configureDirectFile(nameservers, searchDomains)
	default:
		return fmt.Errorf("unknown DNS backend: %d", m.backend)
	}
}

// Teardown removes DNS configuration previously applied by Configure.
func (m *Manager) Teardown() error {
	switch m.backend {
	case backendResolvconf:
		return m.teardownResolvconf()
	case backendResolvectl:
		return m.teardownResolvectl()
	case backendDirectFile:
		return m.teardownDirectFile()
	default:
		return fmt.Errorf("unknown DNS backend: %d", m.backend)
	}
}

// -------------------------------------------------------------------------
// resolvconf backend
// -------------------------------------------------------------------------

func (m *Manager) configureResolvconf(nameservers, searchDomains []string) error {
	// Build the resolv.conf-style input.
	var sb strings.Builder
	for _, ns := range nameservers {
		fmt.Fprintf(&sb, "nameserver %s\n", ns)
	}
	if len(searchDomains) > 0 {
		fmt.Fprintf(&sb, "search %s\n", strings.Join(searchDomains, " "))
	}

	// -a <iface>: add config for interface
	// -m 0: highest metric (highest priority)
	// -x: use exclusive mode (only our servers for matching domains)
	//nolint:gosec // interface name is controlled by us
	cmd := exec.Command("resolvconf", "-a", "tun."+m.ifName, "-m", "0", "-x")
	cmd.Stdin = strings.NewReader(sb.String())
	if out, err := cmd.CombinedOutput(); err != nil {
		return fmt.Errorf("resolvconf -a: %w: %s", err, out)
	}
	return nil
}

func (m *Manager) teardownResolvconf() error {
	//nolint:gosec // interface name is controlled by us
	cmd := exec.Command("resolvconf", "-d", "tun."+m.ifName, "-f")
	if out, err := cmd.CombinedOutput(); err != nil {
		return fmt.Errorf("resolvconf -d: %w: %s", err, out)
	}
	return nil
}

// -------------------------------------------------------------------------
// resolvectl backend (systemd-resolved)
// -------------------------------------------------------------------------

func (m *Manager) configureResolvectl(nameservers, searchDomains []string) error {
	// Set DNS servers for the interface.
	args := append([]string{"dns", m.ifName}, nameservers...)
	//nolint:gosec // arguments are controlled by us
	if out, err := exec.Command("resolvectl", args...).CombinedOutput(); err != nil {
		return fmt.Errorf("resolvectl dns: %w: %s", err, out)
	}

	// Set search domains (prefixed with ~ for routing-only domains).
	routingDomains := make([]string, len(searchDomains))
	for i, d := range searchDomains {
		routingDomains[i] = "~" + d
	}
	args = append([]string{"domain", m.ifName}, routingDomains...)
	//nolint:gosec // arguments are controlled by us
	if out, err := exec.Command("resolvectl", args...).CombinedOutput(); err != nil {
		return fmt.Errorf("resolvectl domain: %w: %s", err, out)
	}

	return nil
}

func (m *Manager) teardownResolvectl() error {
	//nolint:gosec // interface name is controlled by us
	if out, err := exec.Command("resolvectl", "revert", m.ifName).CombinedOutput(); err != nil {
		return fmt.Errorf("resolvectl revert: %w: %s", err, out)
	}
	return nil
}

// -------------------------------------------------------------------------
// Direct /etc/resolv.conf backend (last resort)
// -------------------------------------------------------------------------

const (
	resolvConfPath     = "/etc/resolv.conf"
	resolvConfBackup   = "/etc/resolv.conf.coder-backup"
	managedBlockHeader = "# BEGIN coder-desktop managed block"
	managedBlockFooter = "# END coder-desktop managed block"
)

func (m *Manager) configureDirectFile(nameservers, searchDomains []string) error {
	// Backup existing resolv.conf.
	existing, err := os.ReadFile(resolvConfPath)
	if err != nil && !os.IsNotExist(err) {
		return fmt.Errorf("read %s: %w", resolvConfPath, err)
	}

	if len(existing) > 0 {
		if err := os.WriteFile(resolvConfBackup, existing, 0o644); err != nil {
			return fmt.Errorf("backup %s: %w", resolvConfPath, err)
		}
		m.backupPath = resolvConfBackup
		log.Printf("dns: backed up %s to %s", resolvConfPath, resolvConfBackup)
	}

	// Build managed block.
	var sb strings.Builder
	sb.WriteString(managedBlockHeader + "\n")
	for _, ns := range nameservers {
		fmt.Fprintf(&sb, "nameserver %s\n", ns)
	}
	if len(searchDomains) > 0 {
		fmt.Fprintf(&sb, "search %s\n", strings.Join(searchDomains, " "))
	}
	sb.WriteString(managedBlockFooter + "\n")

	// Prepend our block before existing content.
	newContent := sb.String() + string(existing)
	if err := os.WriteFile(resolvConfPath, []byte(newContent), 0o644); err != nil {
		return fmt.Errorf("write %s: %w", resolvConfPath, err)
	}

	return nil
}

func (m *Manager) teardownDirectFile() error {
	if m.backupPath == "" {
		// No backup — try to remove our managed block from the file.
		return m.removeManagedBlock()
	}

	// Restore from backup.
	backup, err := os.ReadFile(m.backupPath)
	if err != nil {
		return fmt.Errorf("read backup %s: %w", m.backupPath, err)
	}
	if err := os.WriteFile(resolvConfPath, backup, 0o644); err != nil {
		return fmt.Errorf("restore %s: %w", resolvConfPath, err)
	}
	_ = os.Remove(m.backupPath)
	m.backupPath = ""
	log.Printf("dns: restored %s from backup", resolvConfPath)
	return nil
}

func (m *Manager) removeManagedBlock() error {
	data, err := os.ReadFile(resolvConfPath)
	if err != nil {
		return nil // nothing to clean up
	}

	content := string(data)
	startIdx := strings.Index(content, managedBlockHeader)
	endIdx := strings.Index(content, managedBlockFooter)
	if startIdx < 0 || endIdx < 0 {
		return nil // no managed block found
	}

	// Remove from start of header to end of footer line.
	endIdx += len(managedBlockFooter)
	if endIdx < len(content) && content[endIdx] == '\n' {
		endIdx++
	}
	cleaned := content[:startIdx] + content[endIdx:]
	return os.WriteFile(resolvConfPath, []byte(cleaned), 0o644)
}

// TailscaleConfigurator implements tailscale's dns.OSConfigurator interface
// for the Coder VPN. It mirrors the linuxDNSConfigurator from dns_linux.go
// in the bridge.go c-shared library.
//
// SupportsSplitDNS() returns false so tailscale's DNS manager calls
// GetBaseConfig() and uses those as the fallback forwarder. The actual
// OS-level DNS reconfiguration is handled by the Manager above.
type TailscaleConfigurator struct{}

func (TailscaleConfigurator) SetDNS(tsdns.OSConfig) error { return nil }
func (TailscaleConfigurator) SupportsSplitDNS() bool      { return false }
func (TailscaleConfigurator) GetBaseConfig() (tsdns.OSConfig, error) {
	return tsdns.OSConfig{
		Nameservers: []netip.Addr{
			netip.MustParseAddr("8.8.8.8"),
			netip.MustParseAddr("8.8.4.4"),
			netip.MustParseAddr("1.1.1.1"),
		},
	}, nil
}
func (TailscaleConfigurator) Close() error { return nil }
