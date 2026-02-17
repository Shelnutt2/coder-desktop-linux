// Package sdutil wraps github.com/mdlayher/sdnotify for simple systemd
// readiness notification.
package sdutil

import "github.com/mdlayher/sdnotify"

// NotifyReady sends the READY=1 notification to systemd.
// Returns nil if NOTIFY_SOCKET is not set (i.e., not running under systemd).
func NotifyReady() error {
	n, err := sdnotify.New()
	if err != nil {
		// If NOTIFY_SOCKET is not set, New() returns an error.
		// This is expected when not running under systemd.
		return err
	}
	defer n.Close()
	return n.Notify(sdnotify.Ready)
}
