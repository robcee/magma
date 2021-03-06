/*
Copyright (c) Facebook, Inc. and its affiliates.
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

package servicers

import (
	"errors"
	"net"
	"net/url"
	"strconv"
	"strings"
	"unsafe"

	"github.com/ishidawataru/sctp"
)

const (
	VLRAddrEnv          = "VLR_ADDR"
	DefaultVLRIPAddress = "127.0.0.1"
	DefaultVLRPort      = 1357
	LocalIPAddress      = "127.0.0.1"
	LocalPort           = 0
)

type SCTPClientConnection struct {
	sendConn    *sctp.SCTPConn
	vlrSCTPAddr *sctp.SCTPAddr
}

func NewSCTPClientConnection(vlrIP string, vlrPort int) (*SCTPClientConnection, error) {
	return &SCTPClientConnection{
		vlrSCTPAddr: ParseAddr(vlrIP, vlrPort),
	}, nil
}

func (conn *SCTPClientConnection) EstablishConn() error {
	sendConn, err := sctp.DialSCTP(
		"sctp",
		nil,
		conn.vlrSCTPAddr,
	)
	if err != nil {
		return err
	}
	conn.sendConn = sendConn

	return nil
}

func (conn *SCTPClientConnection) CloseConn() error {
	if conn.sendConn == nil {
		return errors.New("connection to VLR not established")
	}

	err := conn.sendConn.Close()
	conn.sendConn = nil
	if err != nil {
		return err
	}

	return nil
}

func (conn *SCTPClientConnection) Send(message []byte) error {
	if conn.sendConn == nil {
		return errors.New("connection to VLR not established")
	}

	ppid := 0

	info := &sctp.SndRcvInfo{
		Stream: uint16(ppid),
		PPID:   uint32(ppid),
	}

	conn.sendConn.SubscribeEvents(sctp.SCTP_EVENT_DATA_IO)
	_, err := conn.sendConn.SCTPWrite(message, info)
	if err != nil {
		return err
	}

	return nil
}

func (conn *SCTPClientConnection) Receive() ([]byte, error) {
	if conn.sendConn == nil {
		return []byte{}, errors.New("connection to VLR not established")
	}

	buf := make([]byte, 254)
	n, _, err := conn.sendConn.SCTPRead(buf)
	if err != nil {
		return []byte{}, err
	}

	return buf[:n], err
}

type SCTPServerConnection struct {
	rcvListener     *sctp.SCTPListener
	sendConn        *sctp.SCTPConn
	infoWrappedConn *sctp.SCTPSndRcvInfoWrappedConn
}

func NewSCTPServerConnection() (*SCTPServerConnection, error) {
	return &SCTPServerConnection{
		rcvListener: nil,
		sendConn:    nil,
	}, nil
}

func (conn *SCTPServerConnection) StartListener(ipAddr string, port PortNumber) (PortNumber, error) {
	ln, err := sctp.ListenSCTP("sctp", ParseAddr(ipAddr, port))
	if err != nil {
		return -1, err
	}

	conn.rcvListener = ln
	address := url.URL{Host: ln.Addr().String()}
	portNumber, err := strconv.Atoi(address.Port())
	if err != nil {
		return -1, err
	}

	return portNumber, nil
}

func (conn *SCTPServerConnection) CloseListener() error {
	err := conn.rcvListener.Close()
	if err != nil {
		return err
	}

	return nil
}

func (conn *SCTPServerConnection) ConnectionEstablished() bool {
	if conn.sendConn == nil {
		return false
	}
	return true
}

func (conn *SCTPServerConnection) AcceptConn() error {
	netConn, err := conn.rcvListener.Accept()
	if err != nil {
		return err
	}
	conn.infoWrappedConn = sctp.NewSCTPSndRcvInfoWrappedConn(netConn.(*sctp.SCTPConn))
	conn.sendConn = netConn.(*sctp.SCTPConn)
	return nil
}

func (conn *SCTPServerConnection) CloseConn() error {
	if conn.sendConn == nil {
		return errors.New("connection to client not established")
	}
	err := conn.sendConn.Close()
	conn.sendConn = nil
	conn.infoWrappedConn = nil
	if err != nil {
		return err
	}
	return nil
}

func (conn *SCTPServerConnection) ReceiveThroughListener() ([]byte, error) {
	if conn.infoWrappedConn == nil {
		return []byte{}, errors.New("connection to client not established")
	}

	buf := make([]byte, 254)
	n, err := conn.infoWrappedConn.Read(buf)

	if err != nil {
		return []byte{}, err
	}

	return buf[unsafe.Sizeof(sctp.SndRcvInfo{}):n], err
}

func (conn *SCTPServerConnection) SendFromServer(msg []byte) error {
	if conn.sendConn == nil {
		return errors.New("connection to client not established")
	}

	err := conn.sendConn.SubscribeEvents(sctp.SCTP_EVENT_DATA_IO)
	if err != nil {
		return err
	}

	ppid := 0
	info := &sctp.SndRcvInfo{
		Stream: uint16(ppid),
		PPID:   uint32(ppid),
	}

	_, err = conn.sendConn.SCTPWrite(msg, info)
	if err != nil {
		return err
	}

	return nil
}

func ParseAddr(ip string, port int) *sctp.SCTPAddr {
	ips := []net.IPAddr{}
	for _, i := range strings.Split(ip, ",") {
		if a, err := net.ResolveIPAddr("ip", i); err == nil {
			ips = append(ips, *a)
		}
	}

	return &sctp.SCTPAddr{
		IPAddrs: ips,
		Port:    port,
	}
}
