"""
Use this in the same way as Python's SimpleHTTPServer:

  python -m RangeHTTPServer [port]

The only difference from SimpleHTTPServer is that RangeHTTPServer supports
'Range:' headers to load portions of files. This is helpful for doing local web
development with genomic data files, which tend to be to large to load into the
browser all at once.
"""
import os
import sys
import socket
from zeroconf import ServiceInfo, Zeroconf

try:
    # Python3
    import http.server as SimpleHTTPServer

except ImportError:
    # Python 2
    import SimpleHTTPServer

from RangeHTTPServer import RangeRequestHandler

import argparse

def register( port, desc, servername, mdns):
    zeroconf = Zeroconf()
    ip_list =  ([ip for ip in socket.gethostbyname_ex(socket.gethostname())[2] ])
    host_ip = ip_list[0]
    fqdn = socket.getfqdn(host_ip)
    hostname = fqdn.split('.') [0]

    try:
        host_ip_pton = socket.inet_pton(socket.AF_INET, host_ip)
    except OSError:
        host_ip_pton = socket.inet_pton(socket.AF_INET6, host_ip)
    
    services = []
    type_ = "_http._tcp.local."
    info = ServiceInfo(
        type_,
        mdns + "." + type_,
        addresses=[host_ip_pton],
        port=port,
        properties=desc,
        server=f"{hostname}.local",
    )
    services.append(info)
    for info in services:
        zeroconf.register_service(info)
    return zeroconf, services

def unregister(zeroconf, services):
    print(f"Unregistering {services=} ...")
    for info in services:
        zeroconf.unregister_service(info)
    zeroconf.close()
    print(f"done.")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-p', '--port', action='store',
                        default=8000, type=int,
                        nargs='?', help='Specify alternate port [default: 8000]')
    parser.add_argument('-w', '--web', action='store',
                        default=".",
                        nargs='?', 
                        help='Specify root directory to server from [default .]')
    parser.add_argument('-e', '--encoding', action='store',
                        default="none",
                        nargs='?', 
                        help='Encoding type for server to utilize [default none]')
    parser.add_argument('-m', '--mdns', action='store',
                        help='announce via MDNS')
    parser.add_argument("-t", "--trace", action='store',
                        help="trace arguments and execution")


    args = parser.parse_args()
    if args.web:
        os.chdir(args.web)
    if args.mdns:
        zeroconf, services = register(args.port, {}, "", args.mdns)

    try:
        sys.stderr.write(f"Announcing: {services=}")
        RangeRequestHandler.protocol_version = "HTTP/1.0"
        SimpleHTTPServer.test(HandlerClass=RangeRequestHandler, port=args.port)

    except KeyboardInterrupt:
        if args.mdns:
            sys.stderr.write(f"Unregister: {services=}")
            unregister(zeroconf, services)

if __name__ == '__main__':
    print(f"Hello!")

    main()