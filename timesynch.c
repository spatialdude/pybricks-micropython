/**
 * \addtogroup timesynch
 * @{
 */


/*
 * Copyright (c) 2007, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 * $Id: timesynch.c,v 1.5 2008/01/08 07:54:16 adamdunkels Exp $
 */

/**
 * \file
 *         A simple time synchronization mechanism
 * \author
 *         Adam Dunkels <adam@sics.se>
 */

#include "sys/timesynch.h"
#include "net/rime/rimebuf.h"
#include "net/rime.h"
#include "dev/simple-cc2420.h"

static const struct mac_driver *mac;
static void (* receiver_callback)(const struct mac_driver *);

#include <stdio.h>

static int authority_level;
static rtimer_clock_t offset;

/*---------------------------------------------------------------------------*/
int
timesynch_authority_level(void)
{
  return authority_level;
}
/*---------------------------------------------------------------------------*/
void
timesynch_set_authority_level(int level)
{
  authority_level = level;
}
/*---------------------------------------------------------------------------*/
rtimer_clock_t
timesynch_time(void)
{
  return rtimer_arch_now() + offset;
}
/*---------------------------------------------------------------------------*/
rtimer_clock_t
timesynch_time_to_rtimer(rtimer_clock_t synched_time)
{
  return synched_time - offset;
}
/*---------------------------------------------------------------------------*/
rtimer_clock_t
timesynch_offset(void)
{
  return offset;
}
/*---------------------------------------------------------------------------*/
static int
send_packet(void)
{
  return mac->send();
}
/*---------------------------------------------------------------------------*/
static void
adjust_offset(rtimer_clock_t authoritative_time, rtimer_clock_t local_time)
{
  offset = offset + authoritative_time - local_time;
}
/*---------------------------------------------------------------------------*/
static int
read_packet(void)
{
  int len;

  len = mac->read();

  if(len == 0) {
    return 0;
  }
  
  /* We check the authority level of the sender of the incoming
     packet. If the sending node has a lower authority level than we
     have, we synchronize to the time of the sending node and set our
     own authority level to be one more than the sending node. */
  if(simple_cc2420_authority_level_of_sender < authority_level) {
    
    adjust_offset(simple_cc2420_time_of_departure,
		  simple_cc2420_time_of_arrival);
    if(simple_cc2420_authority_level_of_sender + 1 != authority_level) {
      authority_level = simple_cc2420_authority_level_of_sender + 1;
    }

  }
  
  return len;
}
/*---------------------------------------------------------------------------*/
#if 0
static void
periodic_authority_increase(void *ptr)
{
  /* XXX the authority level should be increased over time except
     for the sink node (which has authority 0). */
}
#endif
/*---------------------------------------------------------------------------*/
static void
set_receive_function(void (* recv)(const struct mac_driver *))
{
  receiver_callback = recv;
}
/*---------------------------------------------------------------------------*/
static int
on(void)
{
  return mac->on();
}
/*---------------------------------------------------------------------------*/
static int
off(void)
{
  return mac->off();
}
/*---------------------------------------------------------------------------*/
static const struct mac_driver timesynch_driver = {
  send_packet,
  read_packet,
  set_receive_function,
  on,
  off,
};
/*---------------------------------------------------------------------------*/
static void
input_packet(const struct mac_driver *d)
{
  if(receiver_callback) {
    receiver_callback(&timesynch_driver);
  }
}
/*---------------------------------------------------------------------------*/
const struct mac_driver *
timesynch_init(const struct mac_driver *d)
{
  mac = d;
  mac->set_receive_function(input_packet);
  mac->on();
  return &timesynch_driver;
}
/*---------------------------------------------------------------------------*/
/** @} */
