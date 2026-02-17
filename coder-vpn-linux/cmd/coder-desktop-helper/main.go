// Command coder-desktop-helper is a privileged D-Bus system service that
// manages the Coder VPN tunnel. It replaces the c-shared Go library
// (libcodervpn.so) with a standalone binary that owns TUN creation, DNS
// configuration, and the WireGuard data plane.
//
// It is activated by systemd and communicates with the unprivileged
// coder-desktop Qt application over the system D-Bus.
package main

import (
	"log"
	"os"
	"os/signal"
	"syscall"

	"github.com/godbus/dbus/v5"

	"github.com/coder/coder-vpn-linux/internal/dbusservice"
	"github.com/coder/coder-vpn-linux/internal/sdutil"
)

func main() {
	log.SetFlags(log.Ltime | log.Lmicroseconds)
	log.Printf("coder-desktop-helper starting")

	// Connect to the system D-Bus.
	conn, err := dbus.ConnectSystemBus()
	if err != nil {
		log.Fatalf("connect to system bus: %v", err)
	}
	defer conn.Close()

	// Export the helper service object.
	svc, err := dbusservice.NewHelperService(conn)
	if err != nil {
		log.Fatalf("export service: %v", err)
	}

	// Request the well-known bus name.
	reply, err := conn.RequestName(dbusservice.BusName, dbus.NameFlagDoNotQueue)
	if err != nil {
		log.Fatalf("request bus name %s: %v", dbusservice.BusName, err)
	}
	if reply != dbus.RequestNameReplyPrimaryOwner {
		log.Fatalf("bus name %s already owned (reply=%d)", dbusservice.BusName, reply)
	}
	log.Printf("acquired bus name %s", dbusservice.BusName)

	// Notify systemd that we are ready (Type=notify).
	if err := sdutil.NotifyReady(); err != nil {
		// Non-fatal: we might not be running under systemd.
		log.Printf("sd_notify READY failed (not running under systemd?): %v", err)
	}

	// Block until SIGTERM or SIGINT.
	sigCh := make(chan os.Signal, 1)
	signal.Notify(sigCh, syscall.SIGTERM, syscall.SIGINT)
	sig := <-sigCh
	log.Printf("received %s, shutting down", sig)

	// Clean shutdown: tear down tunnel if running.
	svc.Shutdown()

	log.Printf("coder-desktop-helper stopped")
}
