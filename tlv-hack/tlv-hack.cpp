/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2013, Regents of the University of California
 *
 * BSD license, See the LICENSE file for more information
 *
 * Author: Alexander Afanasyev <alexander.afanasyev@ucla.edu>
 */

extern "C" {
#define ndn NDN_HANDLE_CANNOT_BE_USED_HERE
#include <ndn-tlv/ndnd.h>
#include <ndn-tlv/ndn.h>
#include <ndn-tlv/charbuf.h>
#include <ndn-tlv/coding.h>

#include "../ndnd/ndnd_private.h"
#undef ndn
}

#include "tlv-hack.h"

#include <ndn-cpp-dev/encoding/tlv.hpp>
#include <ndn-cpp-dev/status-response.hpp>
#include <ndn-cpp-dev/data.hpp>

#include "tlv-to-ndnb.hpp"
#include "ndnb-to-tlv.hpp"

#include <fstream>

using namespace ndn;

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////

extern "C" {

ssize_t
tlv_to_ndnb(const unsigned char *buf, size_t length, struct ndn_charbuf *ndnb)
{
  try {
    Block block(buf, length);

    switch (block.type()) {
    case Tlv::Interest:
        interest_tlv_to_ndnb(block, ndnb);
        break;
    case Tlv::Data:
        data_tlv_to_ndnb(block, ndnb);
        break;
    default:
      return -1;
      break;
    }

    return block.size();
  }
  catch (Tlv::Error &error) {
    // do nothing
  }
  catch (Block::Error &error) {
    // do nothing
  }
  catch (Signature::Error &error) {
    // do nothing
  }
  
  return -1;
}

ssize_t
ndnb_to_tlv(const unsigned char *buf, size_t length, unsigned char *tlvbuf, size_t maxlength)
{
  struct ndn_skeleton_decoder decoder = {0};
  struct ndn_skeleton_decoder *d = &decoder;
  ssize_t dres;
  enum ndn_dtag dtag;
  
  d->state |= NDN_DSTATE_PAUSE;
  dres = ndn_skeleton_decode(d, buf, length);
  if (dres < 0)
    {
      return dres;
    }
  else if (NDN_GET_TT_FROM_DSTATE(d->state) != NDN_DTAG)
    {
      return -2;
    }

  dtag = static_cast<ndn_dtag> (d->numval);
  switch (dtag) {
  case NDN_DTAG_Interest:
    {
      struct ndn_parsed_interest parsed_interest = {0};
      struct ndn_parsed_interest *pi = &parsed_interest;
      struct ndn_indexbuf *comps = ndn_indexbuf_create();
    
      dres = ndn_parse_interest(buf, length, pi, comps);
      if (dres < 0) {
        ndn_indexbuf_destroy(&comps);
        return dres;
      }

      Block b = interest_ndnb_to_tlv(buf, parsed_interest, *comps);
      if (b.size() > maxlength) {
        ndn_indexbuf_destroy(&comps);
        return -10;
      }

      memcpy(tlvbuf, b.wire(), b.size());
      
      ndn_indexbuf_destroy(&comps);
      return b.size();
    }
  case NDN_DTAG_ContentObject:
    {
      struct ndn_parsed_ContentObject obj = {0};
      struct ndn_indexbuf *comps = ndn_indexbuf_create();

      dres = ndn_parse_ContentObject(buf, length, &obj, comps);
      if (dres < 0) {
        ndn_indexbuf_destroy(&comps);
        return dres;
      }

      Block b = data_ndnb_to_tlv(buf, obj, *comps);
      if (b.size() > maxlength) {
        ndn_indexbuf_destroy(&comps);
        return -10;
      }

      memcpy(tlvbuf, b.wire(), b.size());
      
      ndn_indexbuf_destroy(&comps);
      return b.size();
    }
  default:
    break;
  }
  
  return -1;
}

ssize_t
tlv_encode_StatusResponse(struct ndn_charbuf *ndnb, int errcode, const char *errtext)
{
  StatusResponse response(errcode, errtext);
  ssize_t ret = ndn_charbuf_append(ndnb, response.wireEncode().wire(), response.wireEncode().size());

  if (ret < 0)
    return ret;

  return response.wireEncode().size();
}

ssize_t
tlv_data_get_content(const unsigned char *buf, size_t length, const unsigned char **content, size_t *contentSize)
{
  try {
    Block block(buf, length);
    
    Data data;
    data.wireDecode(Block(buf, length));

    // A little bit not efficient: wireDecode will create bunch of temporary shared_ptr's,
    // Moreover, the return value must be a pointer inside the original buffer
    
    *content = buf + (data.getContent().value() - &*data.wireEncode().begin());
    *contentSize = data.getContent().value_size();

    return 0;
  }
  catch (std::runtime_error &err) {
    return -1;
  }
}

} // extern "C"

