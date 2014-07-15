#include "broker/broker.hh"
#include "EndpointImpl.hh"
#include "PeerImpl.hh"
#include "EndpointActor.hh"

broker::Endpoint::Endpoint(std::string name, int flags)
    : p(new Impl{std::move(name), cppa::spawn<broker::EndpointActor>()})
	{
	}

broker::Endpoint::~Endpoint()
	{
	cppa::anon_send(p->endpoint, cppa::atom("quit"));

	for ( auto peer : p->peers )
		if ( peer.second.Remote() )
			cppa::anon_send(peer.second.p->endpoint, cppa::atom("quit"));
	}

int broker::Endpoint::LastErrno() const
	{
	return p->last_errno;
	}

const std::string& broker::Endpoint::LastError() const
	{
	return p->last_error;
	}

bool broker::Endpoint::Listen(uint16_t port, const char* addr)
	{
	try
		{
		cppa::publish(p->endpoint, port, addr);
		}
	catch ( const cppa::bind_failure& e )
		{
		p->last_errno = e.error_code();
		p->last_error = broker::strerror(p->last_errno);
		return false;
		}
	catch ( const std::exception& e )
		{
		p->last_errno = 0;
		p->last_error = e.what();
		return false;
		}

	return true;
	}

broker::Peer broker::Endpoint::AddPeer(std::string addr, uint16_t port,
                                       std::chrono::duration<double> retry)
	{
	auto port_addr = std::pair<std::string, uint16_t>(addr, port);

	for ( auto peer : p->peers )
		if ( peer.second.Remote() && port_addr == peer.second.RemoteTuple() )
			return peer.second;

	auto remote = cppa::spawn<RemoteEndpointActor>(p->endpoint,
	                                               addr, port, retry);

	Peer rval;
	rval.p->endpoint = remote;
	rval.p->remote = true;
	rval.p->remote_tuple = port_addr;
	p->peers[remote] = rval;
	cppa::anon_send(p->endpoint, cppa::atom("peer"), remote);
	// Once the remote proxy actor connects, it will send the other peer msg
	// so the remote endpoint knows about this local endpoint.
	return rval;
	}

broker::Peer broker::Endpoint::AddPeer(const Endpoint& e)
	{
	if ( this == &e )
		return {};

	auto it = p->peers.find(e.p->endpoint);

	if ( it != p->peers.end() )
		return it->second;

	Peer rval;
	rval.p->endpoint = e.p->endpoint;
	p->peers[e.p->endpoint] = rval;
	cppa::anon_send(p->endpoint, cppa::atom("peer"), e.p->endpoint);
	cppa::anon_send(e.p->endpoint, cppa::atom("peer"), p->endpoint);
	return rval;
	}

bool broker::Endpoint::RemPeer(broker::Peer peer)
	{
	if ( ! peer.Valid() )
		return false;

	auto it = p->peers.find(peer.p->endpoint);

	if ( it == p->peers.end() )
		return false;

	p->peers.erase(it);
	cppa::anon_send(p->endpoint, cppa::atom("unpeer"), peer.p->endpoint);
	cppa::anon_send(peer.p->endpoint, cppa::atom("unpeer"), p->endpoint);
	return true;
	}

void broker::Endpoint::Print(std::string topic, std::string msg) const
	{
	// TODO
	}
