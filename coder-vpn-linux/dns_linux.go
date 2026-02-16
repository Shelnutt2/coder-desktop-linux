package main

import (
	"net/netip"

	"tailscale.com/net/dns"
)

// linuxDNSConfigurator implements dns.OSConfigurator for the Linux VPN bridge.
//
// The Coder tailnet tunnel handles .coder (or custom suffix) domains via its
// built-in DNS resolver running on the magic DNS IP.  For all other domains
// (e.g. google.com), we return a set of public nameservers as the catch-all
// forwarder so that compileConfig() installs them in rcfg.Routes["."].
//
// SupportsSplitDNS() returns false so tailscale's DNS manager calls
// GetBaseConfig() and uses those as the fallback forwarder.  The actual
// OS-level DNS reconfiguration (e.g. systemd-resolved) is handled by the
// native C++ shim, not by this Go code.
type linuxDNSConfigurator struct{}

func (linuxDNSConfigurator) SetDNS(dns.OSConfig) error { return nil }
func (linuxDNSConfigurator) SupportsSplitDNS() bool    { return false }
func (linuxDNSConfigurator) GetBaseConfig() (dns.OSConfig, error) {
	return dns.OSConfig{
		Nameservers: []netip.Addr{
			netip.MustParseAddr("8.8.8.8"),
			netip.MustParseAddr("8.8.4.4"),
			netip.MustParseAddr("1.1.1.1"),
		},
	}, nil
}
func (linuxDNSConfigurator) Close() error { return nil }
