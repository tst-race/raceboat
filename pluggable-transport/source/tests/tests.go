
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

package main

import "C"

import (
	"sync"
	golog "log"
	race_pt3 "race_pt3/race_pt3"
)

var SourceDirectory string = ""


func main() {
	serverLinkAddress := "{\"hostname\":\"localhost\",\"port\":26262}"
	clientLinkAddress := "{\"hostname\":\"localhost\",\"port\":26265}"
	userParams := make(map[string]string)
	userParams["hostname"] = "localhost"
	userParams["PluginCommsTwoSixStub.startPort"] = "26262"
	userParams["PluginCommsTwoSixStub.endPort"] = "26264"

	server := race_pt3.NewRaceServer(
		"twoSixDirectCpp",
		"default",
		"twoSixDirectCpp",
		"default", 
		"",
		"",
		serverLinkAddress,  // receiveLinkAddress
		"",
		0,
		race_pt3.DEBUG,
		"",
		"",
		userParams)

	golog.Println("Listen")
	listener, err := server.Listen()
	if err != nil {
		golog.Println("Listen failed")
		return
	}

	var wg sync.WaitGroup
	wg.Add(2)
	
	clientMessage := "Hello from client"
	serverMessage := "Hello from server"
	introduction := ""
	go func() {
		golog.Println("Dial")
		
		userParams := make(map[string]string)
		userParams["hostname"] = "localhost"
		userParams["PluginCommsTwoSixStub.startPort"] = "26265"
		userParams["PluginCommsTwoSixStub.endPort"] = "26267"

		client := race_pt3.NewRaceClient(
			"twoSixDirectCpp",
			"default",
			"twoSixDirectCpp",
			"default",
			"",
			"",
			serverLinkAddress,  // sendLinkAddress
			clientLinkAddress,  // receiveLinkAddress
			"",
			0,
			race_pt3.DEBUG,
			introduction,
			"",
			userParams)

		clientConn, err := client.Dial()
		if err != nil {
			golog.Println("Dial failed")
			wg.Done()
			return
		}

		golog.Println("Client Write")
		clientWriteBuffer := []byte(clientMessage)
		_, err = clientConn.Write(clientWriteBuffer)
		if err != nil {
			golog.Println("Client Write failed")
			wg.Done()
			return
		}

		golog.Println("Client Read")
		var clientReadBuffer [32]byte
		n, err := clientConn.Read(clientReadBuffer[:])
		if err != nil {
			golog.Println("Client Read failed")
			wg.Done()
			return
		}
		recvClientMessage := string(clientReadBuffer[:n])

		if serverMessage == recvClientMessage {
			golog.Println("Server -> Client message success")
		} else {
			golog.Println("Server -> Client message failed")
		}
		wg.Done()
		wg.Wait()
	}()

	go func() {
		golog.Println("Accept")
		serverConn, err := listener.Accept()
		if err != nil {
			golog.Println("Accept failed")
			wg.Done()
			return
		}

		golog.Println("Server Write")
		serverWriteBuffer := []byte(serverMessage)
		_, err = serverConn.Write(serverWriteBuffer)
		if err != nil {
			golog.Println("Server Write failed")
			wg.Done()
			return
		}

		golog.Println("Server Read")
		var serverReadBuffer [32]byte
		n, err := serverConn.Read(serverReadBuffer[:])
		if err != nil {
			golog.Println("Server Read failed ")
			wg.Done()
			return
		}
		recvServerMessage := string(serverReadBuffer[:n])

		if clientMessage == recvServerMessage {
			golog.Println("Client -> Server message success")
		} else {
			golog.Println("Client -> Server message failed")
		}

		wg.Done()
		wg.Wait()
	}()
	wg.Wait()
}
