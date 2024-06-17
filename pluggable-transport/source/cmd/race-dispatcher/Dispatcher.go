
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
	"fmt"
	"io"
	"io/ioutil"
	golog "log"
	"flag"
	"os"
	"os/signal"
	"syscall"
	"net"
	"path"
	/* pt "git.torproject.org/pluggable-transports/goptlib.git" */
	pt "goptlib/goptlib"
	race_pt3 "race_pt3/race_pt3"
	"sync"
	"errors"
	"strconv"
	"strings"
)

func clientSetup(redirectPath string, wg *sync.WaitGroup, stop <-chan int) (launched bool, listeners []net.Listener, clients []*race_pt3.RaceClient) {
	golog.Println("Creating RaceClient")

	// LISTEN ON SOCKS
	// Begin goptlib client process.
	golog.Println("calling ClientSetup")
	ptClientInfo, err := pt.ClientSetup(nil)
	golog.Println("ClientSetup called")
	if err != nil {
		golog.Println(err)
	}
	if ptClientInfo.ProxyURL != nil {
		pt.ProxyError("proxy is not supported")
		os.Exit(1)
	}
	listeners = make([]net.Listener, 0)
	clients = make([]*race_pt3.RaceClient, 0)

	ln, err := pt.ListenSocks("tcp", "127.0.0.1:0")
	if err != nil {
		golog.Println("ListenSocks error")
		pt.CmethodError("ListenSOcks error", err.Error())
	}
	golog.Println("Started SOCKS listener at %v.", ln.Addr())

	wg.Add(1)
	defer wg.Done()
	go clientSocksAcceptLoop(ln, clients, redirectPath, wg, stop)
	golog.Println("After go clientSocksAcceptLoop")

	// when using ptadapter: use the "transport" in the config file
	pt.Cmethod("race_pt3", ln.Version(), ln.Addr()) 
	listeners = append(listeners, ln)
	launched = true
	pt.CmethodsDone()
	return launched, listeners, clients
}

func GetLocalIP() string {
	addrs, err := net.InterfaceAddrs()
	golog.Println("addrs: ", addrs)
	if err != nil {
		golog.Println("Err")
		return ""
	}
	for _, address := range addrs {
		// check the address type and if it is not a loopback the display it
		if ipnet, ok := address.(*net.IPNet); ok && !ipnet.IP.IsLoopback() {
			if ipnet.IP.To4() != nil {
				golog.Println("found one", ipnet.IP.String())
				return ipnet.IP.String()
			}
		}
	}
	golog.Println("returning empty-string")
	return ""
}
func clientSocksAcceptLoop(ln *pt.SocksListener, clients []*race_pt3.RaceClient, redirectPath string, wg *sync.WaitGroup, stop <-chan int) {
	// go ACCEPT SOCKS
	defer ln.Close()

	golog.Println("clientSocksAcceptLoop: Entered")
	for {
		wg.Add(1)
		golog.Println("clientSocksAcceptLoop: select")
		select {
		case <-stop:
			defer wg.Done()
			golog.Println("clientSocksAcceptLoop return")
			return
		default:
			golog.Println("clientSocksAcceptLoop: calling AcceptSocks")
			conn, err := ln.AcceptSocks()
			if err != nil {
				golog.Println("clientSocksAcceptLoop: some error")
				if err, ok := err.(net.Error); ok && err.Temporary() {
					golog.Println("clientSocksAcceptLoop: temporary error")
					continue
				}
				golog.Println("SOCKS accept error: %s", err)
				defer wg.Done()
				break
			}
			golog.Printf("SOCKS accepted: %v", conn.Req)
			
			go func() {
				wg.Add(1)
				defer wg.Done()
				defer conn.Close()

				name := "raceproxy"
				role := "default"

				// socksReq, err := socks5.Handshake(conn)
				if err != nil {
					golog.Printf("%s - client failed socks handshake: %s", name, err)
					return
				}

				// TODO - these args don't make sense for twosix another plugin
				// pass user params (e.g. conn.Req.Args)?
				userParams := make(map[string]string)

				hostIP := "localhost"
				hostIP2 := GetLocalIP()
				if hostIP2 != "" {
					hostIP = hostIP2
				}
				userParams["hostname"] = hostIP
				userParams["PluginCommsTwoSixStub.startPort"] = "26265"
				userParams["PluginCommsTwoSixStub.endPort"] = "26267"
			
				golog.Println("Args:")
				for key, arg := range conn.Req.Args {
					golog.Println(key, arg)
					if len(arg) > 0 {
						userParams[key] = strings.Join(arg[:], ",")
					}
				}
				
				sendChannel := "twoSixDirectCpp"
				if arg, ok := conn.Req.Args.Get("send"); ok {
					sendChannel = arg
					golog.Println("send:", sendChannel)
				}
				recvChannel := "twoSixDirectCpp"
				if arg, ok := conn.Req.Args.Get("recv"); ok {
					recvChannel = arg
					golog.Println("recv:", recvChannel)
				}

				sendLinkAddress := "{\"hostname\":\"172.17.0.2\",\"port\":26262}"
				if arg, ok := conn.Req.Args.Get("sendLinkAddress"); ok {
					sendLinkAddress = arg
					golog.Println("Send address:", sendLinkAddress)
				}
				
				recvLinkAddress := ""
				if arg, ok := conn.Req.Args.Get("recvLinkAddress"); ok {
					recvLinkAddress = arg
					golog.Println("Receive address:", recvLinkAddress)
				}

				introMessage := ""
				if arg, ok := conn.Req.Args.Get("introMessage"); ok {
					introMessage = arg
					golog.Println("introMessage:", introMessage)
				}

				var client *race_pt3.RaceClient
				if len(clients) == 0 {
					golog.Println("Calling NewRaceClient")
					client = race_pt3.NewRaceClient(
						sendChannel,
						role,
						recvChannel,
						role,
						"",
						"",
						// sendLinkAddress,
						// recvLinkAddress,
						"",
						0,
						race_pt3.DEBUG, // TODO: Should be INFO in release
						introMessage,
						redirectPath,
						userParams,
					)

					clients = append(clients, client)
				} else {
					golog.Println("Got Existing RaceClient")
					client = clients[0]
				}
				golog.Printf("client address: %p\n", client)  // debug

				golog.Println("Dialing.")
				rconn, err := client.Dial(sendLinkAddress)
				if err != nil {
					golog.Println("ERROR on Dial")
					return
				}
				golog.Println("Dialed.")
				err = conn.Grant(&net.TCPAddr{IP: net.IPv4zero, Port: 0})
				// socksReq.Reply(socks5.ReplySucceeded)
				if err != nil {
					golog.Printf("%s(%s) - SOCKS reply failed: %s", name, err)
					return
				}
				defer rconn.Close()
				copyLoop(conn, rconn)
				golog.Println("Exited copyLoop")
			}()
			wg.Done()
		}
	}
}

func copyLoop(socks, race io.ReadWriter) {
	var copyWg sync.WaitGroup
	copyWg.Add(2)

	go func() {
		golog.Println("Copy Socks to Conduit")
		written, err := io.Copy(race, socks)
		if err != nil {
			golog.Println("io.Copy error " + err.Error())
		} else {
			golog.Println("copied " + strconv.FormatInt(written, 10) + " bytes from Socks to Conduit")
		}
		golog.Println("Done with Socks to Conduit Loop")
		copyWg.Done()
	}()
	go func() {
		golog.Println("Copy Conduit to Socks")
		written, err := io.Copy(socks, race)
		if err != nil {
			golog.Println("io.Copy error " + err.Error())
		} else {
			golog.Println("copied " + strconv.FormatInt(written, 10) + " bytes from Conduit to Socks")
		}
		golog.Println("Done with Conduit to Socks Loop")
		copyWg.Done()
	}()

	copyWg.Wait()
}

func serverSetup(wg *sync.WaitGroup, stop <-chan int, redirectPath string) (launched bool, listeners []race_pt3.RaceListener, servers []*race_pt3.RaceServer) {
	golog.Println("Creating RaceServer")
	ptServerInfo, err := pt.ServerSetup(nil)
	if err != nil {
		golog.Println("error in setup: %s", err)
	}

	servers = make([]*race_pt3.RaceServer, len(ptServerInfo.Bindaddrs))

	for index, bindaddr := range ptServerInfo.Bindaddrs {
		golog.Println("Bindaddr: " + bindaddr.Addr.String())
		name := bindaddr.MethodName
		golog.Println("Name: " + name)

		golog.Println("Options:")
		// TODO - these args don't make sense for twosix another plugin
		// pass user params (e.g. conn.Req.Args)?
		userParams := make(map[string]string)
		
		hostIP := "localhost"
		hostIP2 := GetLocalIP()
		if hostIP2 != "" {
			hostIP = hostIP2
		}
		userParams["hostname"] = hostIP
		userParams["PluginCommsTwoSixStub.startPort"] = "26265"
		userParams["PluginCommsTwoSixStub.endPort"] = "26267"
		
		golog.Println("Args:")
		for key, arg := range bindaddr.Options {
			golog.Println(key, arg)
			if len(arg) > 0 {
				userParams[key] = strings.Join(arg[:], ",")
			}
		}
		sendChannel := "twoSixDirectCpp"
		if arg, ok := bindaddr.Options.Get("send"); ok {
			sendChannel = arg
			golog.Println("send:", sendChannel)
		}
		recvChannel := "twoSixDirectCpp"
		if arg, ok := bindaddr.Options.Get("recv"); ok {
			recvChannel = arg
			golog.Println("recv:", recvChannel)
		}

		role := "default"
		recvLinkAddress := "{\"hostname\":\"localhost\",\"port\":26262}"
		if arg, ok := bindaddr.Options.Get("recvLinkAddress"); ok {
			recvLinkAddress = arg
			golog.Println("recvLinkAddress:", recvLinkAddress)
		}

		wg.Add(1)
		golog.Println("Calling NewRaceServer")
		server := race_pt3.NewRaceServer(
			sendChannel,
			role,
			recvChannel,
			role,
			"",
			"",
			recvLinkAddress,
			"",
			0,
			race_pt3.DEBUG,
			"",
			redirectPath,
			userParams,
		)
		golog.Printf("server address: %p\n", server) // debug
		servers[index] = server
		
		golog.Println("Listening.")
		ln, err := server.Listen()
		wg.Done()
		if err != nil {
			golog.Println("Listen failed.")
			// TODO handle failure, DO NOT start the serverAcceptLoop
		}

		go func() {
			_ = serverAcceptLoop(&ln, &ptServerInfo, wg, stop)
		}()
		golog.Println("Started race listener")

		pt.SmethodArgs(name, bindaddr.Addr, nil)
		// pt.Smethod("raceproxy", ln.Addr())

		listeners = append(listeners, ln)
		launched = true
	}
	pt.SmethodsDone()
	return launched, listeners, servers
}

func serverAcceptLoop(ln *race_pt3.RaceListener, info *pt.ServerInfo, wg *sync.WaitGroup, stop <-chan int) error {
	defer ln.Close()
	golog.Println("ACCEPT LOOP")
	for {
		wg.Add(1)
		select {
		case <-stop:
			defer wg.Done()
			golog.Println("serverAcceptLoop return")
			return errors.New("serverAcceptLoop done")
		default:
			conn, err := ln.Accept()
			if err != nil {
				wg.Done()
				if e, ok := err.(net.Error); ok && !e.Temporary() {
					return err
				}
				// continue
			}
			go serverHandler(&conn, info)
			wg.Done()
		}
	}
	golog.Println("EXIT ACCEPT LOOP")
	return nil
}

func serverHandler(rconn *race_pt3.RaceConn, info *pt.ServerInfo) {
	defer rconn.Close()
	name := "raceproxy"

	// PAUL this should be a dynamic port eventually
	orConn, err := pt.DialOr(info, "127.0.0.1:2345", name)
	if err != nil {
		golog.Printf("%s(%s) - failed to connect to ORPort: %s", name)
		return
	}

	defer orConn.Close()
	copyLoop(orConn, rconn)
	golog.Println("ENDED copyLoop")
}

func getVersion() string {
	return fmt.Sprintf("raceproxy-%s", "0.0.1")
}

func main() {
	// Initialize the termination state monitor as soon as possible.
	// termMon = newTermMonitor()

	// Handle the command line arguments.
	_, execName := path.Split(os.Args[0])
	isClient, err := ptIsClient()
	if err != nil {
		golog.Println("[ERROR]: %s - must be run as a managed transport", execName)
	}
	var logPath string
	if isClient {
		logPath = "/log/dispatcher-client.log"
	} else {
		logPath = "/log/dispatcher-server.log"
	}

	f, err := os.OpenFile(logPath, os.O_RDWR|os.O_CREATE|os.O_APPEND, 0666)
	if err != nil {
		golog.Println("error opening file: %v", err)
	}
	defer f.Close()

	golog.SetOutput(f)
	golog.Println("START DISPATCHER")
	for _, entry := range os.Environ() {
		golog.Println(entry)
	}

	var stdoutRedirectPathPtr *string = nil
	showVer := flag.Bool("version", false, "Print version and exit")

	if isClient {
		stdoutRedirectPathPtr = flag.String("redirect-path", "/etc/race/logging/core/client.log", "PT API uses stdout.  Where should RACE logging redirect to?")
	} else {
		stdoutRedirectPathPtr = flag.String("redirect-path", "/etc/race/logging/core/server.log", "PT API uses stdout.  Where should RACE logging redirect to?")
	}

	flag.Parse()

	if *showVer {
		fmt.Printf("%s\n", getVersion())
		os.Exit(0)
	}

	var ptListeners []race_pt3.RaceListener
	var launched bool
	var servers []*race_pt3.RaceServer
	var clients []*race_pt3.RaceClient
	var clientWaitGroup sync.WaitGroup
	var serverWaitGroup sync.WaitGroup

	stop := make(chan int)  // unbuffered int channel defaults to 0

	// Do the managed pluggable transport protocol configuration.
	if isClient {
		golog.Println("%s - initializing client transport listeners", execName)
		launched, _, clients = clientSetup(*stdoutRedirectPathPtr, &clientWaitGroup, stop)
	} else {
		golog.Println("%s - initializing server transport listeners", execName)
		launched, ptListeners, servers = serverSetup(&serverWaitGroup, stop, *stdoutRedirectPathPtr)
	}
	if !launched {
		// Initialization failed, the client or server setup routines should
		// have logged, so just exit here.
		os.Exit(-1)
	}

	golog.Println("%s - accepting connections", execName)
	defer func() {
		golog.Println("%s - terminated", execName)
	}()

	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGTERM)

	if os.Getenv("TOR_PT_EXIT_ON_STDIN_CLOSE") == "1" {
		// This environment variable means we should treat EOF on stdin
		// just like SIGTERM: https://bugs.torproject.org/15435.
		go func() {
			if _, err := io.Copy(ioutil.Discard, os.Stdin); err != nil {
				golog.Printf("calling io.Copy(ioutil.Discard, os.Stdin) returned error: %v", err)
			}
			golog.Printf("synthesizing SIGTERM because of stdin close")
			sigChan <- syscall.SIGTERM
		}()
	}

	// Wait for a signal.
	<-sigChan
	golog.Println("stopping raceproxy")
	for _, ln := range ptListeners {
		ln.Close()
	}

	// inform server and client to stop to safely delete the RaceClient and RaceServer's
	stop <- 1

	clientWaitGroup.Wait()
	for _, client := range clients {
		golog.Printf("destroying client: %p\n", client) // debug
		race_pt3.DestroyRaceClient(client)
	}

	serverWaitGroup.Wait()
	for _, server := range servers {
		golog.Printf("destroying server: %p\n", server) // debug
		race_pt3.DestroyRaceServer(server)
	}

	golog.Println("Done.")
	// termMon.wait(true)
}
