//
// Copyright 2023 Two Six Technologies
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

// CorePluginTwoSix Interface - Implements PT API to allow for use of RACE channels.

package race_pt3

import "C"

import (
	"errors"
	golog "log"
	core "twosix/core"
)

const (
	DEBUG   int = 0
	INFO    int = 1
	WARNING int = 2
	ERROR   int = 3
)

type RaceClient struct {
	race               core.RaceSwig
	params             core.ChannelParamStore
	sendChannelId      string
	sendChannelRole    string `default:"default"`
	recvChannelId      string
	recvChannelRole    string `default:"default"`
	altChannelId       string
	altChannelRole     string `default:"default"`
	timeoutMs          int  // 0 for no timeout
	introduction       string
}

func NewRaceClient(
	sendChannelId string,
	sendChannelRole string,
	recvChannelId string,
	recvChannelRole string,
	altChannelId string,
	altChannelRole string,
	pluginPath string,
	timeoutMs int,
	logLevel int,
	introduction string,
	redirectPath string,
	userParams map[string]string) *RaceClient {

	if len(redirectPath) > 0 {
		core.RaceLogSetLogRedirectPath(redirectPath)
	}

	golog.Println("NewRaceClient")

	core.RaceLogSetLogLevelFile(core.RaceLogLogLevel(logLevel))
	core.RaceLogSetLogLevelStdout(core.RaceLogLL_NONE)  // PT API uses stdout

	if len(pluginPath) == 0 {
		pluginPath = "/etc/race/plugins"
		golog.Println("using default plugin path " + pluginPath)
	}

	if len(sendChannelId) == 0 && len(recvChannelId) == 0 {
		golog.Println("empty send and receive channel IDs")
		return nil
	}

	params := core.NewChannelParamStore()
	for key, value := range userParams {
		golog.Println("client user param " + key + ":" + value)
		params.SetChannelParam(key, value)
	}

	golog.Println("Creating RACE client")
	return &RaceClient{
		race:               core.NewRaceSwig(pluginPath, params),
		params:             params,
		sendChannelId:      sendChannelId,
		sendChannelRole:    sendChannelRole,
		recvChannelId:      recvChannelId,
		recvChannelRole:    recvChannelRole,
		altChannelId:       altChannelId,
		altChannelRole:     altChannelRole,
		timeoutMs:          timeoutMs,	
		introduction:       introduction,
	}
}

func DestroyRaceClient(raceClient *RaceClient) {
	golog.Println("DestroyRaceClient")
	if raceClient.race != nil {
		core.DeleteRaceSwig(raceClient.race)
		raceClient.race = nil
	}
	if raceClient.params != nil {
		core.DeleteChannelParamStore(raceClient.params)
		raceClient.params = nil
	}
}

type RaceConn struct {
	conn  core.SwigConduit
	race  core.RaceSwig
}

func (client RaceClient) Dial(sendAddress string) (RaceConn, error) {
	golog.Println("Calling Dial")
	// Create/load
	// Open connection
	// Return net.Conn mapping itself to this particular connectionId
	sendOpts := core.NewSendOptions()
	defer core.DeleteSendOptions(sendOpts)
	sendOpts.SetRecv_channel(client.recvChannelId)
	sendOpts.SetSend_channel(client.sendChannelId)
	sendOpts.SetAlt_channel(client.altChannelId)
	sendOpts.SetSend_address(sendAddress)
	sendOpts.SetSend_role(client.sendChannelRole)
	sendOpts.SetRecv_role(client.recvChannelRole)
	sendOpts.SetTimeout_ms(client.timeoutMs)

	// Dial_strSwig() returns wrapped std::pair<ApiStatus, SwigConduit>
	pair := client.race.Dial_strSwig(sendOpts, client.introduction)
	if pair.GetFirst() != core.ApiStatus_OK {
		golog.Println("Dial error")
		return RaceConn{ conn: nil, race: nil, }, errors.New("race_pt3 Dial error")
	}

	return RaceConn{
			conn: pair.GetSecond(),
			race: client.race,
		}, 
		nil
}

func (client RaceClient) Resume(sendAddress string, recvAddress string, packageId string) (RaceConn, error) {
	golog.Println("Calling Resume")
	// Create/load
	// Open connection
	// Return net.Conn mapping itself to this particular connectionId
	resumeOpts := core.NewResumeOptions()
	defer core.DeleteResumeOptions(resumeOpts)
	resumeOpts.SetRecv_channel(client.recvChannelId)
	resumeOpts.SetSend_channel(client.sendChannelId)
	resumeOpts.SetAlt_channel(client.altChannelId)
	resumeOpts.SetSend_address(sendAddress)
	resumeOpts.SetRecv_address(recvAddress)
	resumeOpts.SetSend_role(client.sendChannelRole)
	resumeOpts.SetRecv_role(client.recvChannelRole)
	resumeOpts.SetTimeout_ms(client.timeoutMs)
	resumeOpts.SetPackage_id(packageId)

	// ResumeSwig() returns wrapped std::pair<ApiStatus, SwigConduit>
	pair := client.race.ResumeSwig(resumeOpts)
	if pair.GetFirst() != core.ApiStatus_OK {
		golog.Println("Resume error")
		return RaceConn{ conn: nil, race: nil, }, errors.New("race_pt3 Resume error")
	}

	return RaceConn{
			conn: pair.GetSecond(),
			race: client.race,
		}, 
		nil
}

func (conn RaceConn) Read(buffer []byte) (n int, err error) {
	golog.Println("Calling Read")
	
	data_size := 0
	for {
		result := conn.conn.ReadSwig()
		data_size = int(result.GetResult().Size())
		status := result.GetStatus()
		golog.Printf("Read %d bytes", data_size)

		if status != core.ApiStatus_OK {
			return data_size, errors.New("race_pt3 Read error") // + strconv.Itoa(status))
		}

		if data_size > 0 {
			// copy(buffer, []byte(data))
			for idx := 0; idx < data_size; idx++ {
				buffer[idx] = result.GetResult().Get(idx)
			}
			break
		}
	}

	return data_size, nil
}

func (conn RaceConn) Write(buffer []byte) (n int, err error) {
	golog.Println("Calling Write")
	data := core.NewByteVector()
	// golog.Println(data)
	for _, b := range buffer {
		data.Add(b)
	}

	golog.Printf("Trying to write %d bytes", data.Size())

	// TODO pass in SendOptions
	status := conn.conn.Write(data)
	if status == core.ApiStatus_OK {
		golog.Printf("Write success")
		return int(data.Size()), nil
	}
	return 0, errors.New("race_pt3 Write error")
}

func (conn RaceConn) Close() error {
	golog.Println("Calling Connection Close")
	conn.conn.Close()
	return nil
}

// // *** SERVER SIDE ***
type RaceServer struct {
	race               core.RaceSwig
	params             core.ChannelParamStore
	sendChannelId      string
	sendChannelRole    string `default:"default"`
	recvChannelId      string
	recvChannelRole    string `default:"default"`
	altChannelId       string
	altChannelRole     string `default:"default"`
	receiveLinkAddress string
	timeoutMs          int  // 0 for no timeout
}

func NewRaceServer(
	sendChannelId string, 
	sendChannelRole string, 
	recvChannelId string, 
	recvChannelRole string, 
	altChannelId string, 
	altChannelRole string, 
	receiveLinkAddress string,
	pluginPath string,
	timeoutMs int,
	logLevel int,
	introduction string,
	redirectPath string,
	userParams map[string]string) *RaceServer {
		
	if len(redirectPath) > 0 {
		core.RaceLogSetLogRedirectPath(redirectPath)
	}

	golog.Println("NewRaceServer")

	core.RaceLogSetLogLevelFile(core.RaceLogLogLevel(logLevel))
	core.RaceLogSetLogLevelStdout(core.RaceLogLL_NONE)   // PT API uses stdout
	
	if len(pluginPath) == 0 {
		pluginPath = "/etc/race/plugins"
		golog.Println("using default plugin path " + pluginPath)
	}

	if len(receiveLinkAddress) == 0 {
		golog.Println("null receive link addresses")
		return nil
	}
	if len(sendChannelId) == 0 && len(recvChannelId) == 0 {
		golog.Println("null send and receive channel IDs")
		return nil
	}

	params := core.NewChannelParamStore()
	for key, value := range userParams {
		golog.Println("server user param " + key + ":" + value)
		params.SetChannelParam(key, value)
	}

	golog.Println("Creating RACE server")
	return &RaceServer{
		race:               core.NewRaceSwig(pluginPath, params),
		params:             params,
		sendChannelId:      sendChannelId,
		sendChannelRole:    sendChannelRole,
		recvChannelId:      recvChannelId,
		recvChannelRole:    recvChannelRole,
		altChannelId:       altChannelId,
		altChannelRole:     altChannelRole,
		receiveLinkAddress: receiveLinkAddress,	
		timeoutMs:          timeoutMs,	
	}
}

func DestroyRaceServer(raceServer *RaceServer) {
	golog.Println("DestroyRaceServer")
	if raceServer.race != nil {
		core.DeleteRaceSwig(raceServer.race)
		raceServer.race = nil
	}
	if raceServer.params != nil {
		core.DeleteChannelParamStore(raceServer.params)
		raceServer.params = nil
	}
}

type RaceListener struct {
	linkAddress string
	listener  core.SwigAcceptObject
	race      core.RaceSwig
	conns     core.SwigConnVector
}

func (server *RaceServer) Listen() (RaceListener, error) {
	golog.Println("Calling Listen")
	recvOpts := core.NewReceiveOptions()
	defer core.DeleteReceiveOptions(recvOpts)
	recvOpts.SetRecv_channel(server.recvChannelId)
	recvOpts.SetSend_channel(server.sendChannelId)
	recvOpts.SetAlt_channel(server.altChannelId)
	recvOpts.SetRecv_address(server.receiveLinkAddress)
	recvOpts.SetSend_role(server.sendChannelRole)
	recvOpts.SetRecv_role(server.recvChannelRole)
	recvOpts.SetTimeout_ms(server.timeoutMs)

	// ListenSwig returns ListenResult { status, linkAddr, acceptObject }
	result := server.race.ListenSwig(recvOpts)
	if result.GetStatus() != core.ApiStatus_OK {
		golog.Println("Listen error")
		return RaceListener{}, errors.New("race_pt3 Listen error")
	}

	return RaceListener{
			linkAddress: result.GetLinkAddr(), 
			listener: result.GetAcceptObject(), 
			race: server.race, 
			conns: core.NewSwigConnVector(),
			}, nil
}

func (listener RaceListener) Accept() (RaceConn, error) {
	golog.Println("Calling Accept")

	// AcceptSwig() returns std::pair<ApiStatus, SwigConduit>
	if listener.listener != nil {
		result := listener.listener.AcceptSwig(0)
		if result.GetFirst() != core.ApiStatus_OK {
			golog.Println("Accept error")
			return RaceConn{}, errors.New("race_pt3 Accept error")
		}
		golog.Println("AcceptSwig Returned")
		listener.conns.Add(result.GetSecond())
		return RaceConn{conn: result.GetSecond(), race: listener.race}, nil
	} 
	return RaceConn{}, errors.New("race_pt3 nil listener")
}

func (listener RaceListener) Close() error {
	golog.Println("Calling Listener Close")
	if listener.conns != nil {
		var connsCount = listener.conns.Size()
		var ix int = 0
		for; ix < int(connsCount); ix++ {
			if listener.conns.Get(ix).Close() != core.ApiStatus_OK {
				golog.Println("Listener Connection Close error")
			}
		}
		listener.conns.Clear()
		core.DeleteSwigConnVector(listener.conns)
	}
	return nil
}

func (listener RaceListener) Addr() string {
	return listener.linkAddress
}
