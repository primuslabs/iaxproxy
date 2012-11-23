INTRODUCTION

IAXProxy was created to allow Service Providers the freedom to integrate IAX2 based end-points seamlessly into a SIP environment.  Previously interconnecting IAX based devices to a SIP based network was challenging at best, requiring the network operator to run dedicated Asterisk PBX's to connect these devices.  The result was that IAX2 based users were always "second class citizens" in a SIP environment - the SIP "core" was not aware of the device state of an IAX2 endpoint (registered/unregistered).  IAXProxy changes that by providing "surrogate registration" type functionality for IAX2 devices.  When an IAX2 end point connects to IAXProxy the endpoint information is looked up in Redis and assuming the IAX2 device passes authentication then a SIP Peer and SIP Registrar are created on the users behalf.  When the IAX2 endpoint becomes unreachable the SIP Peer & Registrar are deleted.  This allows the SIP network to be fully aware of the state of IAX2 devices and features such as Call Forwarding Unreachable to be provisioned at the SIP Server level.

To achieve this, IAXProxy relies on proven technologies:

Redis - an open-source, networked, in-memory, key-value data store with optional durability.
Restlet - a lightweight, comprehensive, open source REST framework for Java.
Asterisk - IAXProxy utilizes the proven IAX and SIP protocol stacks from Asterisk

NEWS

23 Nov 2012 — Initial Public Release (version 0.2.1.3, Blue Moon release)

LICENSE

IAXProxy is free software and distributed under the GNU General Public License Version 2.