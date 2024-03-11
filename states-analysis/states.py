import sys
import json
from typing import Optional


class Address:
    def __init__(self,
                 link,
                 channel,
                 isolated):
        type(channel)
        self.channel = channel
        self.link = link
        self.isolated = isolated

class Channel:
    def __init__(self, name, directionality):
        self.name = name
        self.directionality = directionality

    def __repr__(self):
        return f"{self.name}-{self.directionality}"

class Link:
    def __init__(self,
                 link,
                 channel,
                 send,
                 recv,
                 isolated=False,
                 ready=False):
        self.channel = channel
        self.send = send
        self.recv = recv
        self.isolated = isolated
        self.ready = ready
        self.link = link

    def __repr__(self):
        dir_symbol = ''
        if self.recv:
            dir_symbol += '<-R-'
        if self.send:
            dir_symbol += '-S->'
            
        if self.ready:
            dir_symbol = dir_symbol.replace("-", "=")

        if self.isolated:
            dir_symbol = f"({dir_symbol})"
        else:
            dir_symbol = f" {dir_symbol} "


        return f"{self.link}: {dir_symbol}"

    def __gt__(self, other):
        return (
            self.ready and
            self.channel.name == other.channel.name and
            ((not other.isolated) or self.isolated) and
            ((not other.send) or self.send) and
            ((not other.recv) or self.recv) 
        )

    def __lt__(self, other):
        return other.__gt__(self)

    def __eq__(self, other):
        return self.__gt__(other) and other.__gt__(self)
from pprint import pprint, pp
global link_idx
link_idx = 1
class NodeState:
    def __init__(self, requirements):
        self.links = {}
        self.other_node = None
        self.requirements = requirements

    def start(self, args):
        pass
    def finished_or(self, status, messages):
        if self.is_final_state(self.requirements):
            return ("FINISHED", messages)
        else:
            return status, messages

 
    def step(self, msgs):
        print(f"Starting Step, State:")
        print(f"{msgs=}")
        [print(l) for l in self.links.values()]
        
        for msg in msgs:
            self.load_link(msg)

        if len(msgs) > 0:
            print("After Loading:")
            [print(l) for l in self.links.values()]

        addrs = []
        # TODO - needs to try other links even if final links can't be done yet
        if self.can_send():
            for required in self.requirements:
                if not any([link > required for link in self.links.values()]):
                    print(f"Missing: {required}")
                    if ((required.channel.directionality == "BIDI") or
                        (required.channel.directionality == "L2C" and required.recv) or
                        (required.channel.directionality == "C2L" and required.send)):
                        addrs.append(self.create_link(required.channel, isolated=True))
                        
            if len(addrs) > 0:
                return self.finished_or("CREATED NEW LINKS", addrs)

        return self.finished_or ("STUCK", [])

 
    def satisfies(self, requirements):
        return all([any([link > req
                        for link in self.links.values()])
                    for req in requirements])


    def is_final_state(self, required):
        return self.satisfies(required)


    def load_link(self, addr):
        new_link = Link(addr.link,
                        addr.channel,
                        is_send(addr.channel, loaded=True),
                        is_recv(addr.channel, loaded=True),
                        addr.isolated
                        )
        self.links[addr.link] = new_link
        self.links[addr.link].ready = True
        self.other_node.links[addr.link].ready = True


    def create_link(self, channel, isolated):
        global link_idx
        link = f"{channel.name}-link-{link_idx}"
        new_link = Link(link,
                        channel,
                        is_send(channel, loaded=False),
                        is_recv(channel, loaded=False),
                        isolated
                        )
        link_idx += 1
        self.links[new_link.link] = new_link
        addr = new_link
        return addr


    def can_send(self):
        return any([link.ready and link.send for link in self.links.values()])
 

    def establish_link(self, channel, creator):
        addr = self.create_link(creator, channel, True)
        if self.can_send(creator):
            self.load_link(addr)
            return True

        else:
            print(f"ERROR: cannot establish link from {creator=} due to inability to send")
            
        return False

class Client(NodeState):
    def start(self, args):
        addrs = []
        if args.init_send.directionality in ["C2L"]:
            # create a link and share its address to the server
            addr = self.create_link(args.init_send, isolated=False)
            # not isolated because OOB shared addr is assumed seen by multiple servers
            addrs.append(addr)
        if args.init_recv.directionality in ["L2C"]:
            addr = self.create_link(args.init_recv, isolated=False)
            addrs.append(addr)

        return addrs

 
class Server(NodeState):
    def start(self, args):
        addrs = []
        if args.init_recv.directionality in ["L2C", "BIDI"]:
            # create a link and share its address to the client
            addrs.append(self.create_link(args.init_recv, isolated=False))
            # not isolated because OOB shared addr is assumed seen by multiple clients
        if args.init_send.directionality in ["C2L"]:
            addrs.append(self.create_link(args.init_send, isolated=False))

        return addrs
            

"""
If there's a msg, load it
If there's a link I want that I can create, create it and send it
If I can't create the link and/or I can't send the address, return STUCK and wait
"""

 
def is_send(channel, loaded):
    directionality = channel.directionality
    if directionality == 'BIDI':
        return True
    
    if directionality == 'L2C':
        return loaded

    if directionality == 'C2L':
        return not loaded

def is_recv(channel, loaded):
    return is_send(channel, loaded=not loaded)


class Args:
    def __init__(
            self,
            init_send: Channel,
            init_recv: Channel,
            final_send: Channel,
            final_recv: Channel,
            oob_addr1: Optional[Link] = None,
            oob_addr2: Optional[Link] = None
    ):
        self.init_send = init_send
        self.init_recv = init_recv
        self.final_send = final_send
        self.final_recv = final_recv
        self.oob_addr1 = oob_addr1
        self.oob_addr2 = oob_addr2

 
def scenario1():
    channel = Channel("chanA", "BIDI")
    server = NodeState()
    client = NodeState()
    server.other_node = client
    client.other_node = server

    requirements = [
        Link(0,
             channel,
             send=True,
             recv=False,
             isolated=False,
             ready=True
             ),
        Link(0,
             channel,
             send=False,
             recv=True,
             isolated=False,
             ready=True
             )
    ]
    addr = server.create_link(channel, False)
    client.load_link(addr)
    assert server.is_final_state(requirements)
    assert client.is_final_state(requirements)
    print("FIN")

    
def make_args(init_c2s, init_s2c, final_c2s, final_s2c):
    return (Args(init_s2c, init_c2s, final_s2c, final_c2s),
            Args(init_c2s, init_s2c, final_c2s, final_c2s))
    

def make_requirements(init_c2s, init_s2c, final_c2s, final_s2c, isolated=True):
    server_requirements = [
        Link(f"{init_s2c.name}",
             init_s2c,
             send=True,
             recv=False,
             isolated=False,
             ready=True
             ),
        Link(f"{init_c2s.name}",
             init_c2s,
             send=False,
             recv=True,
             isolated=False,
             ready=True
             ),
        Link(f"{final_s2c.name}",
             final_s2c,
             send=True,
             recv=False,
             isolated=isolated,
             ready=True
             ),
        Link(f"{final_c2s.name}",
             final_c2s,
             send=False,
             recv=True,
             isolated=isolated,
             ready=True
             )
    ]

    client_requirements = [
        Link(f"{init_c2s.name}",
             init_c2s,
             send=True,
             recv=False,
             isolated=False,
             ready=True
             ),
        Link(f"{init_s2c.name}",
             init_s2c,
             send=False,
             recv=True,
             isolated=False,
             ready=True
             ),
        Link(f"{final_c2s.name}",
             final_c2s,
             send=True,
             recv=False,
             isolated=isolated,
             ready=True
             ),
        Link(f"{final_s2c.name}",
             final_s2c,
             send=False,
             recv=True,
             isolated=isolated,
             ready=True
             )
    ]

    return server_requirements, client_requirements

    
    
    
def run(args, reqs, client_oob=True):
    server_reqs, client_reqs = reqs

    server = Server(server_reqs)
    client = Client(client_reqs)
    server.other_node = client
    client.other_node = server

    server_args, client_args = args
    server_msgs = server.start(server_args)
    client_msgs = client.start(client_args)

    if not client_oob:
        client_msgs = []

    idx = 0
    while not client.is_final_state(client_reqs) or not server.is_final_state(server_reqs):
        print("---\nSERVER")
        status, msgs = server.step(client_msgs)
        client_msgs = []
        print(f"{status=}")
        server_msgs.extend(msgs)
                
        print("---\nCLIENT")
        status, msgs = client.step(server_msgs)
        server_msgs = []
        print(f"{status=}")
        client_msgs.extend(msgs)

        idx += 1
        if idx > 5:
            print("MAX ITERATIONS REACHED")
            status = "FAILED"
            break

    
    print("FIN")
    return (server.is_final_state(server_reqs) and
            client.is_final_state(client_reqs))

"""
Explicit list of combinations we don't expect to work AND WHY
"""
def prune_invalid_params(paramsets):
    filters = []
    """
    Initial Client->Server is creator-to-loader, but we do not allow
    any OOB information from client-to-server. The client is expected
    to always initiate the connection and this is impossible

    Example bad params:
    '{"init_c2s": "C2L", "init_s2c": "BIDI", "final_c2s": "BIDI", "final_s2c": "BIDI", "client_oob": false}'
    """
    filters.append(lambda params:
                   not (params["init_c2s"] == "C2L" and
                        params["client_oob"] == False)
                   )



    for f in filters:
        paramsets = filter(f, paramsets)

    return paramsets
    

def generate_scenarios():
    directionalities = ["BIDI", "C2L", "L2C"]
    parameter_space = {
        'init_c2s': directionalities,
        'init_s2c': directionalities,
        'final_c2s': directionalities,
        'final_s2c': directionalities,
        'client_oob': [True, False],
    }
    import itertools as it

    paramsets = [dict(zip(parameter_space.keys(), v)) for v in
                 it.product(*parameter_space.values())]

    paramsets = prune_invalid_params(paramsets)

    for idx, params in enumerate(paramsets):
        print(f'\n\n====={idx}=====\n')
        run_params(params)

def run_params(params):
    init_c2s = Channel(f"^c2s{params['init_c2s']}", params['init_c2s'])
    init_s2c = Channel(f"^s2c{params['init_s2c']}", params['init_s2c'])
    final_c2s = Channel(f"$c2s{params['final_c2s']}", params['final_c2s'])
    final_s2c = Channel(f"$s2c{params['final_s2c']}", params['final_s2c'])
    
    print(f"'{json.dumps(params)}'")
    success = run(make_args(init_c2s, init_s2c, final_c2s, final_s2c),
                  make_requirements(init_c2s, init_s2c, final_c2s, final_s2c),
                  client_oob=params['client_oob'])
    
    if not success:
        print('[ FAILED ]')
    else:
        print('[ PASSED ]')
        



if __name__ == "__main__":
    if len(sys.argv) == 1:
        generate_scenarios()
    else:
        params = json.loads(sys.argv[1])
        run_params(params)

        
        
    # if sys.argv[1] == '1':
    #     scenario1()
    # if sys.argv[1] == '2':
    #     scenario2()
    # if sys.argv[1] == '3':
    #     scenario3()
    # if sys.argv[1] == '4':
    #     scenario4()
   
