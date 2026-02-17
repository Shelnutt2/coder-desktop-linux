// Package dbusservice implements the com.coder.Desktop.Helper1 D-Bus service
// that manages the Coder VPN tunnel.
package dbusservice

import (
	"fmt"
	"log"

	"github.com/godbus/dbus/v5"
)

// polkitAction is the Polkit action checked for privileged operations.
const polkitAction = "com.coder.Desktop.Helper.manage-vpn"

// checkPolkit asks the local PolicyKit authority whether sender is authorized
// for the given action. Interactive authentication (AllowUserInteraction) is
// requested so that a password prompt can appear if needed.
func checkPolkit(conn *dbus.Conn, sender string, action string) *dbus.Error {
	log.Printf("[polkit] CheckAuthorization: sender=%q action=%q", sender, action)

	obj := conn.Object(
		"org.freedesktop.PolicyKit1",
		"/org/freedesktop/PolicyKit1/Authority",
	)

	// PolkitSubject: (sa{sv})
	type polkitSubject struct {
		Kind    string
		Details map[string]dbus.Variant
	}

	subject := polkitSubject{
		Kind: "system-bus-name",
		Details: map[string]dbus.Variant{
			"name": dbus.MakeVariant(sender),
		},
	}

	// Flags: 1 = AllowUserInteraction
	call := obj.Call(
		"org.freedesktop.PolicyKit1.Authority.CheckAuthorization", 0,
		subject, action, map[string]string{}, uint32(1), "",
	)
	if call.Err != nil {
		return &dbus.Error{
			Name: "com.coder.Desktop.Helper.Error.PolkitFailed",
			Body: []interface{}{call.Err.Error()},
		}
	}

	if len(call.Body) < 1 {
		return &dbus.Error{
			Name: "com.coder.Desktop.Helper.Error.PolkitFailed",
			Body: []interface{}{"empty response from PolicyKit"},
		}
	}

	// The response is a struct (bba{ss}) decoded by godbus as []interface{}.
	// The first element is the isAuthorized boolean.
	resultSlice, ok := call.Body[0].([]interface{})
	if !ok || len(resultSlice) < 1 {
		return &dbus.Error{
			Name: "com.coder.Desktop.Helper.Error.PolkitFailed",
			Body: []interface{}{fmt.Sprintf("unexpected polkit result type: %T", call.Body[0])},
		}
	}

	isAuth, ok := resultSlice[0].(bool)
	if !ok {
		return &dbus.Error{
			Name: "com.coder.Desktop.Helper.Error.PolkitFailed",
			Body: []interface{}{fmt.Sprintf("unexpected polkit isAuthorized type: %T", resultSlice[0])},
		}
	}

	if !isAuth {
		return &dbus.Error{
			Name: "org.freedesktop.DBus.Error.AccessDenied",
			Body: []interface{}{"Polkit authorization denied for action: " + action},
		}
	}

	return nil
}

// makeDBusError creates a *dbus.Error with the given name and message body.
func makeDBusError(name, msg string) *dbus.Error {
	return &dbus.Error{
		Name: name,
		Body: []interface{}{msg},
	}
}
