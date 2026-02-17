package dbusservice

import (
	"context"
	"fmt"
	"log"
	"net/http"
	"net/netip"
	"net/url"
	"os/exec"
	"sync"
	"time"

	cslog "cdr.dev/slog/v3"
	"github.com/coder/coder/v2/codersdk"
	"github.com/coder/coder/v2/codersdk/workspacesdk"
	"github.com/coder/coder/v2/tailnet"
	tailnetproto "github.com/coder/coder/v2/tailnet/proto"
	"github.com/coder/quartz"
	"github.com/coder/websocket"
	"github.com/godbus/dbus/v5"
	"github.com/godbus/dbus/v5/introspect"
	"github.com/tailscale/wireguard-go/tun"
	"tailscale.com/net/tsaddr"

	internaldns "github.com/coder/coder-vpn-linux/internal/dns"
)

const (
	// D-Bus interface and path constants.
	BusName   = "com.coder.Desktop.Helper"
	ObjPath   = "/com/coder/Desktop/Helper"
	Interface = "com.coder.Desktop.Helper1"

	// tunName is the name of the TUN device created by the helper.
	tunName = "coder0"
	// tunMTU is the MTU for the TUN device (matches bridge.go).
	tunMTU = 1280
)

// introspectXML is the D-Bus introspection XML for the Helper1 interface.
const introspectXML = `<node>
  <interface name="` + Interface + `">
    <method name="Start">
      <arg name="coderURL" type="s" direction="in"/>
      <arg name="apiToken" type="s" direction="in"/>
    </method>
    <method name="Stop"/>
    <method name="GetStatus">
      <arg name="state" type="s" direction="out"/>
      <arg name="coderURL" type="s" direction="out"/>
    </method>
    <signal name="StateChanged">
      <arg name="state" type="s"/>
      <arg name="errorMsg" type="s"/>
    </signal>
    <signal name="PeerUpdated">
      <arg name="workspace" type="s"/>
      <arg name="agent" type="s"/>
      <arg name="hostname" type="s"/>
      <arg name="status" type="i"/>
      <arg name="lastPingMs" type="i"/>
      <arg name="isP2P" type="b"/>
    </signal>
    <signal name="LogMessage">
      <arg name="level" type="i"/>
      <arg name="message" type="s"/>
    </signal>
  </interface>` + introspect.IntrospectDataString + `</node>`

// HelperService implements the com.coder.Desktop.Helper1 D-Bus interface.
// It manages the full lifecycle of a Coder VPN tunnel.
type HelperService struct {
	mu       sync.Mutex
	conn     *dbus.Conn
	state    string // "disconnected", "connecting", "connected", "error"
	coderURL string

	// Active tunnel state (non-nil when state == "connected").
	cancel     context.CancelFunc
	tailnetCon *tailnet.Conn
	tunDev     tun.Device
	controller *tailnet.Controller
	dnsManager *internaldns.Manager
}

// NewHelperService creates a new HelperService and exports it on the given
// D-Bus connection. Callers should request the bus name after this returns.
func NewHelperService(conn *dbus.Conn) (*HelperService, error) {
	svc := &HelperService{
		conn:  conn,
		state: "disconnected",
	}

	if err := conn.Export(svc, ObjPath, Interface); err != nil {
		return nil, fmt.Errorf("export service: %w", err)
	}
	if err := conn.Export(
		introspect.Introspectable(introspectXML),
		ObjPath,
		"org.freedesktop.DBus.Introspectable",
	); err != nil {
		return nil, fmt.Errorf("export introspection: %w", err)
	}

	return svc, nil
}

// Start begins a VPN tunnel to the given Coder deployment. It authenticates,
// creates a TUN device, and starts the full tailnet control plane.
//
//nolint:revive // dbus.Sender must be the first parameter for godbus
func (s *HelperService) Start(sender dbus.Sender, coderURL string, apiToken string) *dbus.Error {
	log.Printf("[dbus] Start called: sender=%q coderURL=%q", string(sender), coderURL)
	if dbusErr := checkPolkit(s.conn, string(sender), polkitAction); dbusErr != nil {
		return dbusErr
	}

	s.mu.Lock()
	if s.state == "connected" || s.state == "connecting" {
		s.mu.Unlock()
		return makeDBusError(Interface+".Error.AlreadyRunning", "tunnel is already running")
	}
	s.state = "connecting"
	s.coderURL = coderURL
	s.mu.Unlock()

	s.emitStateChanged("connecting", "")

	// Run the blocking tunnel setup in a goroutine so the D-Bus method
	// returns immediately. Errors are communicated via StateChanged signals.
	go s.startTunnel(coderURL, apiToken)
	return nil
}

// Stop tears down the active VPN tunnel.
//
//nolint:revive // dbus.Sender must be the first parameter for godbus
func (s *HelperService) Stop(sender dbus.Sender) *dbus.Error {
	if dbusErr := checkPolkit(s.conn, string(sender), polkitAction); dbusErr != nil {
		return dbusErr
	}
	s.shutdown()
	return nil
}

// GetStatus returns the current state and Coder URL.
func (s *HelperService) GetStatus() (string, string, *dbus.Error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	return s.state, s.coderURL, nil
}

// Shutdown tears down the tunnel (if running). Safe to call multiple times.
func (s *HelperService) Shutdown() {
	s.shutdown()
}

func (s *HelperService) shutdown() {
	s.mu.Lock()
	defer s.mu.Unlock()

	if s.state == "disconnected" {
		return
	}

	log.Printf("helper: shutting down tunnel")

	// Close tailnet conn first to gracefully drain yamux sessions
	// before canceling the context. This prevents noisy
	// "yamux: Failed to read header" errors on shutdown.
	if s.tailnetCon != nil {
		if err := s.tailnetCon.Close(); err != nil {
			log.Printf("helper: error closing tailnet conn: %v", err)
		}
		s.tailnetCon = nil
	}
	if s.cancel != nil {
		s.cancel()
		s.cancel = nil
	}
	if s.tunDev != nil {
		if err := s.tunDev.Close(); err != nil {
			log.Printf("helper: error closing TUN device: %v", err)
		}
		s.tunDev = nil
	}
	if s.dnsManager != nil {
		if err := s.dnsManager.Teardown(); err != nil {
			log.Printf("helper: error tearing down DNS: %v", err)
		}
		s.dnsManager = nil
	}
	s.controller = nil
	s.state = "disconnected"
	s.emitStateChangedLocked("disconnected", "")
}

// configureTUN brings up the TUN interface and adds addresses and routes using
// the `ip` command. We run as root so these commands succeed without privilege
// escalation. Routes are automatically cleaned up by the kernel when the TUN
// device is closed.
func configureTUN(ifName string, addresses []netip.Prefix, routes []netip.Prefix) error {
	// Bring the link up.
	if err := runIP("link", "set", ifName, "up", "mtu", fmt.Sprintf("%d", tunMTU)); err != nil {
		return fmt.Errorf("bring link up: %w", err)
	}

	// Add addresses.
	for _, addr := range addresses {
		flag := "-6"
		if addr.Addr().Is4() {
			flag = "-4"
		}
		if err := runIP(flag, "addr", "add", addr.String(), "dev", ifName); err != nil {
			return fmt.Errorf("add address %s: %w", addr, err)
		}
	}

	// Add routes.
	for _, route := range routes {
		flag := "-6"
		if route.Addr().Is4() {
			flag = "-4"
		}
		if err := runIP(flag, "route", "add", route.String(), "dev", ifName); err != nil {
			return fmt.Errorf("add route %s: %w", route, err)
		}
	}

	return nil
}

// runIP executes an `ip` command with the given arguments and logs it.
func runIP(args ...string) error {
	log.Printf("helper: running: ip %v", args)
	cmd := exec.Command("ip", args...)
	if out, err := cmd.CombinedOutput(); err != nil {
		return fmt.Errorf("ip %v: %w: %s", args, err, string(out))
	}
	return nil
}

// startTunnel runs the blocking tunnel setup. Mirrors the flow from bridge.go
// CoderVPN_Start (steps 1–9) but uses Go-native TUN creation.
func (s *HelperService) startTunnel(coderURL, apiToken string) {
	logger := cslog.Make(journalSink{conn: s.conn})

	// Step 1: Create codersdk client.
	serverURL, err := url.Parse(coderURL)
	if err != nil {
		s.setError(fmt.Sprintf("parse coder URL: %v", err))
		return
	}
	sdk := codersdk.New(serverURL)
	sdk.SetSessionToken(apiToken)

	ctx, cancel := context.WithCancel(context.Background())

	// Step 2: Authenticate.
	me, err := sdk.User(ctx, codersdk.Me)
	if err != nil {
		cancel()
		s.setError(fmt.Sprintf("get current user: %v", err))
		return
	}
	log.Printf("helper: authenticated as %s (%s)", me.Username, me.ID)

	// Step 3: Get connection info (DERP map, settings).
	connInfo, err := workspacesdk.New(sdk).AgentConnectionInfoGeneric(ctx)
	if err != nil {
		cancel()
		s.setError(fmt.Sprintf("get connection info: %v", err))
		return
	}
	log.Printf("helper: DERP regions: %d, forceWS: %v, disableDirect: %v",
		len(connInfo.DERPMap.Regions), connInfo.DERPForceWebSockets, connInfo.DisableDirectConnections)

	hostnameSuffix := connInfo.HostnameSuffix
	if hostnameSuffix == "" {
		hostnameSuffix = "coder"
	}

	// Step 4: Compute network settings.
	coderServiceIP := tailnet.CoderServicePrefix.RandomAddr()
	tailscaleIP := tailnet.TailscaleServicePrefix.RandomAddr()

	dnsServers := []string{tsaddr.CoderServiceIPv6().String()}
	searchDomains := []string{hostnameSuffix}

	// Step 5: Create TUN device (we run as root, so this works directly).
	tunDev, err := tun.CreateTUN(tunName, tunMTU)
	if err != nil {
		cancel()
		s.setError(fmt.Sprintf("create TUN device: %v", err))
		return
	}
	log.Printf("helper: created TUN device %s (mtu=%d)", tunName, tunMTU)

	// Step 5b: Configure TUN device (link up, addresses, routes).
	tunAddresses := []netip.Prefix{
		netip.PrefixFrom(coderServiceIP, 128),
		netip.PrefixFrom(tailscaleIP, 128),
	}
	tunRoutes := []netip.Prefix{
		tailnet.TailscaleServicePrefix.AsNetip(),
		netip.MustParsePrefix("100.64.0.0/10"),
		tailnet.CoderServicePrefix.AsNetip(),
	}
	if err := configureTUN(tunName, tunAddresses, tunRoutes); err != nil {
		_ = tunDev.Close()
		cancel()
		s.setError(fmt.Sprintf("configure TUN device: %v", err))
		return
	}

	// Step 6: Configure DNS.
	dnsMgr := internaldns.NewManager(tunName)
	if err := dnsMgr.Configure(dnsServers, searchDomains); err != nil {
		_ = tunDev.Close()
		cancel()
		s.setError(fmt.Sprintf("configure DNS: %v", err))
		return
	}

	// Step 7: Create tailnet.Conn.
	headers := make(http.Header)
	headers.Set(codersdk.SessionTokenHeader, apiToken)

	conn, err := tailnet.NewConn(&tailnet.Options{
		Addresses:           tunAddresses,
		DERPMap:             connInfo.DERPMap,
		DERPHeader:          &headers,
		DERPForceWebSockets: connInfo.DERPForceWebSockets,
		Logger:              logger,
		BlockEndpoints:      connInfo.DisableDirectConnections,
		TUNDev:              tunDev,
		ForceNetworkUp:      true,
		DNSConfigurator:     internaldns.TailscaleConfigurator{},
	})
	if err != nil {
		_ = dnsMgr.Teardown()
		_ = tunDev.Close()
		cancel()
		s.setError(fmt.Sprintf("create tailnet conn: %v", err))
		return
	}

	// Step 8: Set up control plane — Controller with coordination,
	// DERP, resume tokens, and workspace updates.
	rpcURL, err := url.Parse(coderURL)
	if err != nil {
		_ = conn.Close()
		_ = dnsMgr.Teardown()
		_ = tunDev.Close()
		cancel()
		s.setError(fmt.Sprintf("parse RPC URL: %v", err))
		return
	}
	rpcURL.Path = "/api/v2/tailnet"

	dialer := workspacesdk.NewWebsocketDialer(logger, rpcURL, &websocket.DialOptions{
		HTTPClient:      sdk.HTTPClient,
		HTTPHeader:      headers,
		CompressionMode: websocket.CompressionDisabled,
	}, workspacesdk.WithWorkspaceUpdates(&tailnetproto.WorkspaceUpdatesRequest{
		WorkspaceOwnerId: tailnet.UUIDToByteSlice(me.ID),
	}))

	controller := tailnet.NewController(logger, dialer)
	coordCtrl := tailnet.NewTunnelSrcCoordController(logger, conn)
	controller.CoordCtrl = coordCtrl
	controller.DERPCtrl = tailnet.NewBasicDERPController(logger, nil, conn)
	controller.ResumeTokenCtrl = tailnet.NewBasicResumeTokenController(logger, quartz.NewReal())

	// Workspace updates → PeerUpdated D-Bus signals.
	updatesHandler := &dbusUpdatesHandler{
		conn:           s.conn,
		tailnetConn:    conn,
		hostnameSuffix: hostnameSuffix,
		agents:         make(map[string]*trackedAgent),
	}
	updatesCtrl := tailnet.NewTunnelAllWorkspaceUpdatesController(
		logger, coordCtrl,
		tailnet.WithDNS(conn, me.Username, tailnet.DNSNameOptions{Suffix: hostnameSuffix}),
		tailnet.WithHandler(updatesHandler),
	)
	controller.WorkspaceUpdatesCtrl = updatesCtrl

	// Step 9: Start the controller.
	controller.Run(ctx)

	// Start periodic ping monitoring.
	go updatesHandler.pingLoop(ctx)

	// Save tunnel state.
	s.mu.Lock()
	s.cancel = cancel
	s.tailnetCon = conn
	s.tunDev = tunDev
	s.controller = controller
	s.dnsManager = dnsMgr
	s.state = "connected"
	s.mu.Unlock()

	s.emitStateChanged("connected", "")
	log.Printf("helper: tunnel started successfully")

	// Monitor for unexpected controller death.
	go func() {
		<-controller.Closed()
		s.mu.Lock()
		// Only report error if this is still the active tunnel and it
		// wasn't stopped intentionally.
		if s.controller == controller && s.state == "connected" {
			s.state = "error"
			s.emitStateChangedLocked("error", "VPN tunnel connection lost")
		}
		s.mu.Unlock()
	}()
}

// setError transitions to the error state and emits a signal.
func (s *HelperService) setError(msg string) {
	log.Printf("helper: error: %s", msg)
	s.mu.Lock()
	s.state = "error"
	s.mu.Unlock()
	s.emitStateChanged("error", msg)
}

// emitStateChanged emits a StateChanged D-Bus signal.
func (s *HelperService) emitStateChanged(state, errorMsg string) {
	if err := s.conn.Emit(ObjPath, Interface+".StateChanged", state, errorMsg); err != nil {
		log.Printf("helper: failed to emit StateChanged signal: %v", err)
	}
}

// emitStateChangedLocked emits a StateChanged D-Bus signal (caller holds s.mu).
func (s *HelperService) emitStateChangedLocked(state, errorMsg string) {
	// Emit is safe to call while holding the lock — it doesn't call back into us.
	if err := s.conn.Emit(ObjPath, Interface+".StateChanged", state, errorMsg); err != nil {
		log.Printf("helper: failed to emit StateChanged signal: %v", err)
	}
}

// ---------------------------------------------------------------------------
// dbusUpdatesHandler — translates workspace/agent updates into PeerUpdated
// D-Bus signals (mirrors linuxUpdatesHandler from bridge.go).
// ---------------------------------------------------------------------------

type dbusUpdatesHandler struct {
	conn           *dbus.Conn
	tailnetConn    *tailnet.Conn
	hostnameSuffix string

	mu     sync.Mutex
	agents map[string]*trackedAgent
}

type trackedAgent struct {
	workspaceName string
	agentName     string
	hostname      string
	ip            netip.Addr
}

func (h *dbusUpdatesHandler) Update(update tailnet.WorkspaceUpdate) error {
	// Upserted agents → CONNECTED.
	for _, agent := range update.UpsertedAgents {
		workspace := findWorkspaceForAgent(agent, update.UpsertedWorkspaces)
		wsName := ""
		if workspace != nil {
			wsName = workspace.Name
		}
		hostname := formatHostname(agent.Name, wsName, h.hostnameSuffix)
		h.emitPeerUpdated(wsName, agent.Name, hostname, 2, -1, false)

		var agentIP netip.Addr
		for _, addrs := range agent.Hosts {
			if len(addrs) > 0 {
				agentIP = addrs[0]
				break
			}
		}
		key := wsName + "." + agent.Name
		h.mu.Lock()
		h.agents[key] = &trackedAgent{
			workspaceName: wsName,
			agentName:     agent.Name,
			hostname:      hostname,
			ip:            agentIP,
		}
		h.mu.Unlock()
	}

	// Deleted agents → DISCONNECTED.
	for _, agent := range update.DeletedAgents {
		workspace := findWorkspaceForAgent(agent, update.DeletedWorkspaces)
		wsName := ""
		if workspace != nil {
			wsName = workspace.Name
		}
		hostname := formatHostname(agent.Name, wsName, h.hostnameSuffix)
		h.emitPeerUpdated(wsName, agent.Name, hostname, 0, -1, false)

		key := wsName + "." + agent.Name
		h.mu.Lock()
		delete(h.agents, key)
		h.mu.Unlock()
	}

	// Upserted workspaces without specific agent updates.
	for _, ws := range update.UpsertedWorkspaces {
		if !hasAgentForWorkspace(ws, update.UpsertedAgents) {
			h.emitPeerUpdated(ws.Name, "", ws.Name+"."+h.hostnameSuffix, 2, -1, false)
		}
	}

	// Deleted workspaces without specific agent updates.
	for _, ws := range update.DeletedWorkspaces {
		if !hasAgentForWorkspace(ws, update.DeletedAgents) {
			h.emitPeerUpdated(ws.Name, "", ws.Name+"."+h.hostnameSuffix, 0, -1, false)
		}
	}

	return nil
}

func (h *dbusUpdatesHandler) emitPeerUpdated(workspace, agent, hostname string, status, lastPingMs int32, isP2P bool) {
	if err := h.conn.Emit(ObjPath, Interface+".PeerUpdated",
		workspace, agent, hostname, status, lastPingMs, isP2P,
	); err != nil {
		log.Printf("helper: failed to emit PeerUpdated signal: %v", err)
	}
}

func (h *dbusUpdatesHandler) pingLoop(ctx context.Context) {
	ticker := time.NewTicker(5 * time.Second)
	defer ticker.Stop()

	for {
		select {
		case <-ctx.Done():
			return
		case <-ticker.C:
			h.pingAllAgents(ctx)
		}
	}
}

func (h *dbusUpdatesHandler) pingAllAgents(ctx context.Context) {
	h.mu.Lock()
	agents := make([]*trackedAgent, 0, len(h.agents))
	for _, a := range h.agents {
		agents = append(agents, a)
	}
	h.mu.Unlock()

	for _, agent := range agents {
		if !agent.ip.IsValid() {
			continue
		}

		pingCtx, cancel := context.WithTimeout(ctx, 3*time.Second)
		duration, p2p, _, err := h.tailnetConn.Ping(pingCtx, agent.ip)
		cancel()

		if err != nil {
			continue
		}

		h.emitPeerUpdated(agent.workspaceName, agent.agentName, agent.hostname,
			2, int32(duration.Milliseconds()), p2p)
	}
}

// ---------------------------------------------------------------------------
// Helpers (mirrors bridge.go helpers)
// ---------------------------------------------------------------------------

func findWorkspaceForAgent(agent *tailnet.Agent, workspaces []*tailnet.Workspace) *tailnet.Workspace {
	for _, ws := range workspaces {
		if ws.ID == agent.WorkspaceID {
			return ws
		}
	}
	return nil
}

func hasAgentForWorkspace(ws *tailnet.Workspace, agents []*tailnet.Agent) bool {
	for _, agent := range agents {
		if agent.WorkspaceID == ws.ID {
			return true
		}
	}
	return false
}

func formatHostname(agentName, workspaceName, suffix string) string {
	if workspaceName == "" {
		return agentName + "." + suffix
	}
	return agentName + "." + workspaceName + ".me." + suffix
}

// ---------------------------------------------------------------------------
// journalSink — slog sink that emits LogMessage D-Bus signals and logs
// to stderr (picked up by systemd journal).
// ---------------------------------------------------------------------------

type journalSink struct {
	conn *dbus.Conn
}

func (s journalSink) LogEntry(_ context.Context, e cslog.SinkEntry) {
	level := int32(0) // DEBUG
	switch {
	case e.Level >= cslog.LevelError:
		level = 3
	case e.Level >= cslog.LevelWarn:
		level = 2
	case e.Level >= cslog.LevelInfo:
		level = 1
	}

	msg := e.Message
	log.Printf("codervpn [%d]: %s", level, msg)

	// Also emit as a D-Bus signal for clients that want to display logs.
	_ = s.conn.Emit(ObjPath, Interface+".LogMessage", level, msg)
}

func (s journalSink) Sync() {}


