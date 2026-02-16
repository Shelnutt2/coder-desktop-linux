// Package main builds a C shared library (libcodervpn.so) that exposes the
// Coder tailnet VPN stack to a native Linux application.  It mirrors the
// Android gomobile bridge (coder-vpn-android/bridge.go) but uses cgo
// //export functions and C function-pointer callbacks instead of Java
// interfaces.
//
// Build:
//
//	CGO_ENABLED=1 go build -buildmode=c-shared -o libcodervpn.so .
package main

/*
#include <stdint.h>
#include <stdlib.h>

// Callback invoked when the Go tunnel needs network settings applied.
// The native side should create/reconfigure a TUN device with these
// parameters and return its file descriptor (or -1 on error).
typedef int32_t (*network_settings_cb)(
    const char* addresses,       // comma-separated CIDR
    const char* dns_servers,     // comma-separated IPs
    const char* search_domains,  // comma-separated
    const char* routes,          // comma-separated CIDR
    int32_t mtu
);

// Callback invoked when a workspace agent peer changes state.
typedef void (*peer_update_cb)(
    const char* workspace_name,
    const char* agent_name,
    const char* hostname,
    int32_t status,              // 0=DISCONNECTED, 1=CONNECTING, 2=CONNECTED
    int64_t last_ping_ms,
    int32_t is_p2p               // 0=relay, 1=direct/P2P
);

// Callback invoked on fatal tunnel errors after Start returns.
typedef void (*error_cb)(const char* message);

// Callback for forwarding Go log messages to the native layer.
typedef void (*log_cb)(int32_t level, const char* message);

// Helper trampolines — cgo cannot call C function pointers directly from
// Go, so we define thin C wrapper functions here.
static inline int32_t call_network_settings_cb(
    network_settings_cb cb,
    const char* addresses,
    const char* dns_servers,
    const char* search_domains,
    const char* routes,
    int32_t mtu
) {
    return cb(addresses, dns_servers, search_domains, routes, mtu);
}

static inline void call_peer_update_cb(
    peer_update_cb cb,
    const char* workspace_name,
    const char* agent_name,
    const char* hostname,
    int32_t status,
    int64_t last_ping_ms,
    int32_t is_p2p
) {
    cb(workspace_name, agent_name, hostname, status, last_ping_ms, is_p2p);
}

static inline void call_error_cb(error_cb cb, const char* message) {
    cb(message);
}

static inline void call_log_cb(log_cb cb, int32_t level, const char* message) {
    cb(level, message);
}
*/
import "C"

import (
	"context"
	"fmt"
	"log"
	"net/http"
	"net/netip"
	"net/url"
	"strings"
	"sync"
	"time"
	"unsafe"

	cslog "cdr.dev/slog/v3"
	"github.com/coder/coder/v2/codersdk"
	"github.com/coder/coder/v2/codersdk/workspacesdk"
	"github.com/coder/coder/v2/tailnet"
	tailnetproto "github.com/coder/coder/v2/tailnet/proto"
	"github.com/coder/quartz"
	"github.com/coder/websocket"
	"github.com/tailscale/wireguard-go/tun"
	"tailscale.com/net/tsaddr"
)

// ---------------------------------------------------------------------------
// Global tunnel state
// ---------------------------------------------------------------------------

var tunnelMu sync.Mutex
var tunnel *tunnelState

type tunnelState struct {
	cancel         context.CancelFunc
	conn           *tailnet.Conn
	tunDev         tun.Device
	controller     *tailnet.Controller
	hostnameSuffix string

	// Callbacks — stored so background goroutines can invoke them.
	onNetworkSettings C.network_settings_cb
	onPeerUpdate      C.peer_update_cb
	onError           C.error_cb
	onLog             C.log_cb
}

// ---------------------------------------------------------------------------
// Exported C API
// ---------------------------------------------------------------------------

//export CoderVPN_Start
func CoderVPN_Start(
	coderURL *C.char,
	apiToken *C.char,
	onNetworkSettings C.network_settings_cb,
	onPeerUpdate C.peer_update_cb,
	onError C.error_cb,
	onLog C.log_cb,
) C.int32_t {
	tunnelMu.Lock()
	defer tunnelMu.Unlock()

	if tunnel != nil {
		logToCallback(onLog, 3, "tunnel already running")
		return -1
	}

	goURL := C.GoString(coderURL)
	goToken := C.GoString(apiToken)
	if goURL == "" || goToken == "" {
		logToCallback(onLog, 3, "coderURL and apiToken must not be empty")
		return -1
	}
	if onNetworkSettings == nil {
		logToCallback(onLog, 3, "onNetworkSettings callback must not be nil")
		return -1
	}

	logToCallback(onLog, 0, fmt.Sprintf("starting tunnel to %s", goURL))

	// Step 1: Create codersdk client.
	serverURL, err := url.Parse(goURL)
	if err != nil {
		logToCallback(onLog, 3, fmt.Sprintf("parse coder URL: %v", err))
		return -1
	}
	sdk := codersdk.New(serverURL)
	sdk.SetSessionToken(goToken)

	ctx, cancel := context.WithCancel(context.Background())

	// Step 2: Authenticate.
	me, err := sdk.User(ctx, codersdk.Me)
	if err != nil {
		cancel()
		logToCallback(onLog, 3, fmt.Sprintf("get current user: %v", err))
		return -1
	}
	logToCallback(onLog, 0, fmt.Sprintf("authenticated as %s (%s)", me.Username, me.ID))

	// Step 3: Get connection info (DERP map, settings).
	connInfo, err := workspacesdk.New(sdk).AgentConnectionInfoGeneric(ctx)
	if err != nil {
		cancel()
		logToCallback(onLog, 3, fmt.Sprintf("get connection info: %v", err))
		return -1
	}
	logToCallback(onLog, 0, fmt.Sprintf("DERP regions: %d, forceWS: %v, disableDirect: %v",
		len(connInfo.DERPMap.Regions), connInfo.DERPForceWebSockets, connInfo.DisableDirectConnections))

	hostnameSuffix := connInfo.HostnameSuffix
	if hostnameSuffix == "" {
		hostnameSuffix = "coder"
	}

	// Step 4: Compute network settings.
	coderServiceIP := tailnet.CoderServicePrefix.RandomAddr()
	tailscaleIP := tailnet.TailscaleServicePrefix.RandomAddr()

	addresses := fmt.Sprintf("%s,%s",
		netip.PrefixFrom(coderServiceIP, 128).String(),
		netip.PrefixFrom(tailscaleIP, 128).String(),
	)
	dnsServers := tsaddr.CoderServiceIPv6().String()
	searchDomains := hostnameSuffix
	routes := strings.Join([]string{
		tailnet.TailscaleServicePrefix.AsNetip().String(),
		"100.64.0.0/10",
		tailnet.CoderServicePrefix.AsNetip().String(),
	}, ",")
	var mtu int32 = 1280

	// Step 5: Push network settings to native side → get back TUN fd.
	cAddresses := C.CString(addresses)
	cDNS := C.CString(dnsServers)
	cSearch := C.CString(searchDomains)
	cRoutes := C.CString(routes)
	defer C.free(unsafe.Pointer(cAddresses))
	defer C.free(unsafe.Pointer(cDNS))
	defer C.free(unsafe.Pointer(cSearch))
	defer C.free(unsafe.Pointer(cRoutes))

	tunFd := C.call_network_settings_cb(onNetworkSettings,
		cAddresses, cDNS, cSearch, cRoutes, C.int32_t(mtu))
	if tunFd < 0 {
		cancel()
		logToCallback(onLog, 3, fmt.Sprintf("native side failed to configure TUN (fd=%d)", tunFd))
		return -1
	}
	logToCallback(onLog, 0, fmt.Sprintf("got TUN fd=%d from native", tunFd))

	// Step 6: Create tun.Device from the file descriptor.
	tunDev, _, err := tun.CreateUnmonitoredTUNFromFD(int(tunFd))
	if err != nil {
		cancel()
		logToCallback(onLog, 3, fmt.Sprintf("create TUN device from fd: %v", err))
		return -1
	}

	// Step 7: Create tailnet.Conn.
	headers := make(http.Header)
	headers.Set(codersdk.SessionTokenHeader, goToken)

	logger := newCallbackLogger(onLog)
	conn, err := tailnet.NewConn(&tailnet.Options{
		Addresses:           []netip.Prefix{netip.PrefixFrom(coderServiceIP, 128)},
		DERPMap:             connInfo.DERPMap,
		DERPHeader:          &headers,
		DERPForceWebSockets: connInfo.DERPForceWebSockets,
		Logger:              logger,
		BlockEndpoints:      connInfo.DisableDirectConnections,
		TUNDev:              tunDev,
		ForceNetworkUp:      true,
		DNSConfigurator:     linuxDNSConfigurator{},
	})
	if err != nil {
		_ = tunDev.Close()
		cancel()
		logToCallback(onLog, 3, fmt.Sprintf("create tailnet conn: %v", err))
		return -1
	}

	// Step 8: Set up control plane — Controller with coordination,
	// DERP, resume tokens, and workspace updates.
	rpcURL, err := url.Parse(goURL)
	if err != nil {
		_ = conn.Close()
		_ = tunDev.Close()
		cancel()
		logToCallback(onLog, 3, fmt.Sprintf("parse RPC URL: %v", err))
		return -1
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

	// Workspace updates → peer update callbacks.
	updatesHandler := &linuxUpdatesHandler{
		onPeerUpdate:   onPeerUpdate,
		hostnameSuffix: hostnameSuffix,
		conn:           conn,
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

	ts := &tunnelState{
		cancel:            cancel,
		conn:              conn,
		tunDev:            tunDev,
		controller:        controller,
		hostnameSuffix:    hostnameSuffix,
		onNetworkSettings: onNetworkSettings,
		onPeerUpdate:      onPeerUpdate,
		onError:           onError,
		onLog:             onLog,
	}
	tunnel = ts

	// Monitor for unexpected controller death.
	go func() {
		<-controller.Closed()
		tunnelMu.Lock()
		current := tunnel
		tunnelMu.Unlock()
		// Only report error if this is still the active tunnel and it
		// wasn't stopped intentionally.
		if current == ts && onError != nil {
			cMsg := C.CString("VPN tunnel connection lost")
			C.call_error_cb(onError, cMsg)
			C.free(unsafe.Pointer(cMsg))
		}
	}()

	logToCallback(onLog, 0, "tunnel started successfully")
	return 0
}

//export CoderVPN_Stop
func CoderVPN_Stop() C.int32_t {
	tunnelMu.Lock()
	defer tunnelMu.Unlock()

	if tunnel == nil {
		return 0
	}

	log.Printf("codervpn: stopping tunnel")

	ts := tunnel
	tunnel = nil // clear first so monitor goroutine doesn't fire error cb

	if ts.cancel != nil {
		ts.cancel()
	}
	if ts.conn != nil {
		if err := ts.conn.Close(); err != nil {
			log.Printf("codervpn: error closing tailnet conn: %v", err)
		}
	}
	if ts.tunDev != nil {
		if err := ts.tunDev.Close(); err != nil {
			log.Printf("codervpn: error closing TUN device: %v", err)
		}
	}

	log.Printf("codervpn: tunnel stopped")
	return 0
}

//export CoderVPN_IsRunning
func CoderVPN_IsRunning() C.int32_t {
	tunnelMu.Lock()
	defer tunnelMu.Unlock()
	if tunnel != nil {
		return 1
	}
	return 0
}

//export CoderVPN_UpdateTunFd
func CoderVPN_UpdateTunFd(newFd C.int32_t) C.int32_t {
	tunnelMu.Lock()
	defer tunnelMu.Unlock()

	if tunnel == nil {
		return -1
	}

	log.Printf("codervpn: updating TUN fd to %d", newFd)

	// Close old TUN device.
	if tunnel.tunDev != nil {
		if err := tunnel.tunDev.Close(); err != nil {
			log.Printf("codervpn: error closing old TUN device: %v", err)
		}
	}

	tunDev, _, err := tun.CreateUnmonitoredTUNFromFD(int(newFd))
	if err != nil {
		log.Printf("codervpn: create TUN from fd: %v", err)
		return -1
	}
	tunnel.tunDev = tunDev
	return 0
}

// ---------------------------------------------------------------------------
// linuxUpdatesHandler — translates workspace/agent updates into peer
// update callbacks to the native C++ layer.
// ---------------------------------------------------------------------------

type linuxUpdatesHandler struct {
	onPeerUpdate   C.peer_update_cb
	hostnameSuffix string
	conn           *tailnet.Conn

	mu     sync.Mutex
	agents map[string]*trackedAgent
}

type trackedAgent struct {
	workspaceName string
	agentName     string
	hostname      string
	ip            netip.Addr
}

func (h *linuxUpdatesHandler) Update(update tailnet.WorkspaceUpdate) error {
	if h.onPeerUpdate == nil {
		return nil
	}

	// Upserted agents → CONNECTED.
	for _, agent := range update.UpsertedAgents {
		workspace := findWorkspaceForAgent(agent, update.UpsertedWorkspaces)
		wsName := ""
		if workspace != nil {
			wsName = workspace.Name
		}
		hostname := formatHostname(agent.Name, wsName, h.hostnameSuffix)
		h.sendPeerUpdate(wsName, agent.Name, hostname, 2, -1, false)

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
		h.sendPeerUpdate(wsName, agent.Name, hostname, 0, -1, false)

		key := wsName + "." + agent.Name
		h.mu.Lock()
		delete(h.agents, key)
		h.mu.Unlock()
	}

	// Upserted workspaces without specific agent updates.
	for _, ws := range update.UpsertedWorkspaces {
		if !hasAgentForWorkspace(ws, update.UpsertedAgents) {
			h.sendPeerUpdate(ws.Name, "", ws.Name+"."+h.hostnameSuffix, 2, -1, false)
		}
	}

	// Deleted workspaces without specific agent updates.
	for _, ws := range update.DeletedWorkspaces {
		if !hasAgentForWorkspace(ws, update.DeletedAgents) {
			h.sendPeerUpdate(ws.Name, "", ws.Name+"."+h.hostnameSuffix, 0, -1, false)
		}
	}

	return nil
}

func (h *linuxUpdatesHandler) sendPeerUpdate(wsName, agentName, hostname string, status int32, pingMs int64, p2p bool) {
	if h.onPeerUpdate == nil {
		return
	}
	cWS := C.CString(wsName)
	cAgent := C.CString(agentName)
	cHost := C.CString(hostname)
	defer C.free(unsafe.Pointer(cWS))
	defer C.free(unsafe.Pointer(cAgent))
	defer C.free(unsafe.Pointer(cHost))

	var isP2P C.int32_t
	if p2p {
		isP2P = 1
	}
	C.call_peer_update_cb(h.onPeerUpdate, cWS, cAgent, cHost, C.int32_t(status), C.int64_t(pingMs), isP2P)
}

func (h *linuxUpdatesHandler) pingLoop(ctx context.Context) {
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

func (h *linuxUpdatesHandler) pingAllAgents(ctx context.Context) {
	h.mu.Lock()
	agents := make([]*trackedAgent, 0, len(h.agents))
	for _, a := range h.agents {
		agents = append(agents, a)
	}
	h.mu.Unlock()

	if h.onPeerUpdate == nil || h.conn == nil {
		return
	}

	for _, agent := range agents {
		if !agent.ip.IsValid() {
			continue
		}

		pingCtx, cancel := context.WithTimeout(ctx, 3*time.Second)
		duration, p2p, _, err := h.conn.Ping(pingCtx, agent.ip)
		cancel()

		if err != nil {
			continue
		}

		h.sendPeerUpdate(agent.workspaceName, agent.agentName, agent.hostname,
			2, duration.Milliseconds(), p2p)
	}
}

// ---------------------------------------------------------------------------
// Helpers
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

// logToCallback sends a log message through the C callback if available.
func logToCallback(cb C.log_cb, level int32, msg string) {
	if cb == nil {
		log.Printf("codervpn [%d]: %s", level, msg)
		return
	}
	cMsg := C.CString(msg)
	C.call_log_cb(cb, C.int32_t(level), cMsg)
	C.free(unsafe.Pointer(cMsg))
}

// newCallbackLogger creates a cdr.dev/slog Logger that forwards to the C log callback.
// For simplicity we use the standard Go logger as a sink and rely on logToCallback
// for the C bridge.  A production version could implement a full slog.Sink.
func newCallbackLogger(cb C.log_cb) cslog.Logger {
	return cslog.Make(callbackSink{cb: cb})
}

// callbackSink implements sloghuman-compatible sink that forwards to the C log callback.
type callbackSink struct {
	cb C.log_cb
}

func (s callbackSink) LogEntry(_ context.Context, e cslog.SinkEntry) {
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
	logToCallback(s.cb, level, msg)
}

func (s callbackSink) Sync() {}

// ---------------------------------------------------------------------------
// Required for c-shared buildmode.
// ---------------------------------------------------------------------------

func main() {}

// Ensure imports compile.
var _ = unsafe.Pointer(nil)
