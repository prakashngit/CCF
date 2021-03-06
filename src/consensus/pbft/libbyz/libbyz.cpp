// Copyright (c) Microsoft Corporation.
// Copyright (c) 1999 Miguel Castro, Barbara Liskov.
// Copyright (c) 2000, 2001 Miguel Castro, Rodrigo Rodrigues, Barbara Liskov.
// Licensed under the MIT license.

#include "libbyz.h"

#include "Client.h"
#include "Replica.h"
#include "Reply.h"
#include "Request.h"
#include "Statistics.h"
#include "globalstate.h"
#include "receive_message_base.h"

#include <random>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

int Byz_init_client(const NodeInfo& node_info, INetwork* network)
{
  pbft::GlobalState::set_client(std::make_unique<Client>(node_info, network));
  return 0;
}

void Byz_reset_client()
{
  pbft::GlobalState::get_client().reset();
}

int Byz_alloc_request(Byz_req* req, int size)
{
  Request* request = new Request((Request_id)0, -1);
  if (request == 0)
  {
    return -1;
  }

  int len;
  req->contents = request->store_command(len);
  req->size = len;
  req->opaque = (void*)request;
  return 0;
}

int Byz_send_request(Byz_req* req, bool ro)
{
  Request* request = (Request*)req->opaque;
  request->request_id() = pbft::GlobalState::get_client().get_rid();
  request->authenticate(req->size, ro);

  bool retval = pbft::GlobalState::get_client().send_request(request);
  return (retval) ? 0 : -1;
}

int Byz_recv_reply(Byz_rep* rep)
{
  Reply* reply = pbft::GlobalState::get_client().recv_reply();
  if (reply == NULL)
  {
    return -1;
  }
  rep->contents = reply->reply(rep->size);
  rep->opaque = reply;
  return 0;
}

int Byz_invoke(Byz_req* req, Byz_rep* rep, bool ro)
{
  if (Byz_send_request(req, ro) == -1)
  {
    return -1;
  }
  return Byz_recv_reply(rep);
}

void Byz_free_request(Byz_req* req)
{
  Request* request = (Request*)req->opaque;
  delete request;
}

void Byz_free_reply(Byz_rep* rep)
{
  Reply* reply = (Reply*)rep->opaque;
  delete reply;
}

void Byz_configure_principals()
{
  pbft::GlobalState::get_node().configure_principals();
}

void Byz_add_principal(const PrincipalInfo& principal_info)
{
  pbft::GlobalState::get_node().add_principal(principal_info);
}

void Byz_start_replica()
{
  pbft::GlobalState::get_replica().recv_start();
  stats.zero_stats();
}

int Byz_init_replica(
  const NodeInfo& node_info,
  char* mem,
  unsigned int size,
  ExecCommand exec,
  INetwork* network,
  pbft::RequestsMap& pbft_requests_map,
  pbft::PrePreparesMap& pbft_pre_prepares_map,
  pbft::PbftStore& store,
  IMessageReceiveBase** message_receiver)
{
  // Initialize random number generator
  pbft::GlobalState::set_replica(std::make_unique<Replica>(
    node_info,
    mem,
    size,
    network,
    pbft_requests_map,
    pbft_pre_prepares_map,
    store));

  if (message_receiver != nullptr)
  {
    *message_receiver = &pbft::GlobalState::get_replica();
  }

  // Register service-specific functions.
  pbft::GlobalState::get_replica().register_exec(exec);

  auto used_bytes = pbft::GlobalState::get_replica().used_state_bytes();
  stats.zero_stats();
  return used_bytes;
}

void Byz_modify(void* mem, int size)
{
  pbft::GlobalState::get_replica().modify(mem, size);
}

void Byz_replica_run()
{
  pbft::GlobalState::get_replica().recv();
}

void Byz_reset_stats()
{
  stats.zero_stats();
}

void Byz_print_stats()
{
  stats.print_stats();
}
