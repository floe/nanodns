nanodns
=======

Ultra-minimal authoritative DNS server, Dyndns replacement

By default, nanodns reads data from /var/lib/nanodns/.
For every domain, put a file into this directory which
has the FQDN as filename (e.g. 'foo.bar.baz.org.', note
the final dot) and contains the IPv4 address of the host.
Make sure the files are world-readable, as nanodns drops
all privileges and changes its UID/GID to "nobody".

Also included is an example PHP script to quickly update these
files from the Web. Some inspiration taken from http://smorgasbord.gavagai.nl/2011/08/homemade-dynamic-dns-service/

