/*
Copyright (c) Facebook, Inc. and its affiliates.
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

package test

import (
	"testing"

	"magma/feg/cloud/go/protos/mconfig"
	definitions "magma/feg/gateway/services/s6a_proxy/servicers"
	"magma/feg/gateway/services/testcore/hss/servicers"

	"github.com/fiorix/go-diameter/diam"
	"github.com/fiorix/go-diameter/diam/avp"
	"github.com/fiorix/go-diameter/diam/datatype"
	"github.com/fiorix/go-diameter/diam/dict"
	"github.com/stretchr/testify/assert"
)

func TestConstructPermanentFailureAnswer(t *testing.T) {
	msg := diam.NewMessage(diam.AuthenticationInformation, diam.RequestFlag, diam.TGPP_S6A_APP_ID, 1, 2, dict.Default)
	serverCfg := &mconfig.DiamServerConfig{
		DestHost:  "magma_host",
		DestRealm: "magma_realm",
	}
	response := servicers.ConstructFailureAnswer(msg, datatype.UTF8String("magma"), serverCfg, 1000)

	assert.Equal(t, msg.Header.CommandCode, response.Header.CommandCode)
	assert.Equal(t, uint8(0), response.Header.CommandFlags)
	assert.Equal(t, uint32(diam.TGPP_S6A_APP_ID), response.Header.ApplicationID)
	assert.Equal(t, uint32(1), response.Header.HopByHopID)
	assert.Equal(t, uint32(2), response.Header.EndToEndID)

	_, err := response.FindAVP(avp.ExperimentalResult, dict.UndefinedVendorID)
	assert.NoError(t, err)

	var aia definitions.AIA
	err = response.Unmarshal(&aia)
	assert.NoError(t, err)
	assert.Equal(t, uint32(1000), aia.ExperimentalResult.ExperimentalResultCode)
	assert.Equal(t, datatype.DiameterIdentity("magma_host"), aia.OriginHost)
	assert.Equal(t, datatype.DiameterIdentity("magma_realm"), aia.OriginRealm)
	assert.Equal(t, "magma", aia.SessionID)
}

func TestConstructSuccessAnswer(t *testing.T) {
	msg := diam.NewMessage(diam.AuthenticationInformation, diam.RequestFlag, diam.TGPP_S6A_APP_ID, 1, 2, dict.Default)
	serverCfg := &mconfig.DiamServerConfig{
		DestHost:  "magma_host",
		DestRealm: "magma_realm",
	}
	response := servicers.ConstructSuccessAnswer(msg, datatype.UTF8String("magma"), serverCfg)

	var aia definitions.AIA
	err := response.Unmarshal(&aia)
	assert.NoError(t, err)
	assert.Equal(t, uint32(diam.Success), aia.ExperimentalResult.ExperimentalResultCode)
	assert.Equal(t, uint32(diam.Success), aia.ResultCode)
	assert.Equal(t, datatype.DiameterIdentity("magma_host"), aia.OriginHost)
	assert.Equal(t, datatype.DiameterIdentity("magma_realm"), aia.OriginRealm)
	assert.Equal(t, "magma", aia.SessionID)
}

func TestAddStandardAnswerAVPS(t *testing.T) {
	msg := diam.NewMessage(diam.AuthenticationInformation, 0, diam.TGPP_S6A_APP_ID, 1, 2, dict.Default)
	serverCfg := &mconfig.DiamServerConfig{
		DestHost:  "magma_host",
		DestRealm: "magma_realm",
	}
	servicers.AddStandardAnswerAVPS(msg, "magma", serverCfg, diam.Success)

	var aia definitions.AIA
	err := msg.Unmarshal(&aia)
	assert.NoError(t, err)
	assert.Equal(t, uint32(diam.Success), aia.ExperimentalResult.ExperimentalResultCode)
	assert.Equal(t, datatype.DiameterIdentity("magma_host"), aia.OriginHost)
	assert.Equal(t, datatype.DiameterIdentity("magma_realm"), aia.OriginRealm)
	assert.Equal(t, "magma", aia.SessionID)
}
