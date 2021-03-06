/*
 * This file is part of the libn2t project.
 * Libn2t is a C++ library transforming network layer into transport layer.
 * Copyright (C) 2018  GreaterFire
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "socket.h"
#include <queue>
#include <lwip/tcp.h>
#include "utils.h"
using namespace std;

namespace Net2Tr {
    class Socket::SocketInternal {
    public:
        tcp_pcb *pcb;
        RecvHandler recv;
        SentHandler sent;
        ErrHandler err;
        unsigned pending_len;
        std::string recv_buf;
        queue<err_t> err_que;
        bool pcb_freed;

        SocketInternal() : pcb(NULL), pending_len(0), pcb_freed(false) {}
        ~SocketInternal() {
            if (pcb != NULL && !pcb_freed) {
                // lwip tcp_err_fn: The corresponding pcb is already freed when this callback is called!
                // unregister callbacks
                tcp_recv(pcb, NULL);
                tcp_sent(pcb, NULL);
                tcp_err(pcb, NULL);
                if (ERR_OK != tcp_close(pcb)) {
                    tcp_abort(pcb);
                }

                pcb = NULL;
            }
        }
    };

    Socket::Socket()
    {
        internal = new SocketInternal();
    }

    Socket::~Socket()
    {
        if (internal != NULL) {
            delete internal;
            internal = NULL;
        }
    }

    void Socket::set_pcb(tcp_pcb *pcb)
    {
        internal->pcb = pcb;
        tcp_nagle_disable(internal->pcb);
        tcp_arg(internal->pcb, internal);
        tcp_recv(internal->pcb, [](void *arg, tcp_pcb *tpcb, pbuf *p, err_t err) -> err_t
        {
            err_t ret_err = ERR_OK;
            SocketInternal *internal = (SocketInternal *) arg;
            if (err != ERR_OK || /* err == ERR_OK */ p == NULL) {
                // The callback function will be passed a NULL pbuf to
                // indicate that the remote host has closed the connection
                internal->pcb_freed = true;
                if (tcp_close(tpcb) != ERR_OK) {
                    // Only return ERR_ABRT if you have called tcp_abort
                    // from within the callback function!
                    tcp_abort(tpcb);
                    ret_err = ERR_ABRT;
                }
            }
            if (p != NULL) {
                string packet = Utils::pbuf_to_str(p);
                pbuf_free(p);
                tcp_recved(internal->pcb, u16_t(packet.size()));

                if (internal->recv) {
                    RecvHandler tmp = internal->recv;
                    internal->recv = RecvHandler();
                    tmp(internal->pcb_freed, packet);
                } else {
                    internal->recv_buf += packet;
                }
            }
            // If the callback function returns ERR_OK or ERR_ABRT it must have
            // freed the pbuf, otherwise it must not have freed it.
            return ret_err;
        });
        tcp_sent(internal->pcb, [](void *arg, tcp_pcb *, u16_t len) -> err_t
        {
            SocketInternal *internal = (SocketInternal *) arg;
            internal->pending_len -= len;
            if (internal->pending_len == 0) {
                SentHandler tmp = internal->sent;
                internal->sent = SentHandler();
                tmp(internal->pcb_freed);
            }
            return ERR_OK;
        });
        tcp_err(internal->pcb, [](void *arg, err_t err)
        {
            SocketInternal *internal = (SocketInternal *) arg;
            internal->pcb_freed = true;
            if (internal->err) {
                ErrHandler tmp = internal->err;
                internal->err = ErrHandler();
                tmp(err);
            } else {
                internal->err_que.push(err);
            }
        });
    }

    void Socket::async_recv(const RecvHandler &handler)
    {
        if (internal->pcb_freed) {
            handler(internal->pcb_freed, string());
            return;
        }
        if (internal->recv_buf.size() == 0) {
            internal->recv = handler;
        } else {
            string tmp = internal->recv_buf;
            internal->recv_buf.clear();
            handler(internal->pcb_freed, tmp);
        }
    }

    void Socket::async_send(const string &packet, const SentHandler &handler)
    {
        if (internal->pcb_freed) {
            handler(internal->pcb_freed);
            return;
        }
        internal->pending_len += packet.size();
        internal->sent = handler;
        tcp_write(internal->pcb, packet.c_str(), u16_t(packet.size()), TCP_WRITE_FLAG_COPY);
        tcp_output(internal->pcb);
    }

    void Socket::async_err(const ErrHandler &handler)
    {
        if (internal->err_que.empty()) {
            internal->err = handler;
        } else {
            err_t err = internal->err_que.front();
            internal->err_que.pop();
            handler(err);
        }
    }

    void Socket::cancel()
    {
        if (internal->pcb != NULL) {
            // unregister callbacks
            tcp_recv(internal->pcb, NULL);
            tcp_sent(internal->pcb, NULL);
            tcp_err(internal->pcb, NULL);
        }
        internal->recv = RecvHandler();
        internal->sent = SentHandler();
        internal->err = ErrHandler();
    }

    std::string Socket::src_addr()
    {
        return ipaddr_ntoa(&internal->pcb->remote_ip);
    }

    uint16_t Socket::src_port()
    {
        return internal->pcb->remote_port;
    }

    std::string Socket::dst_addr()
    {
        return ipaddr_ntoa(&internal->pcb->local_ip);
    }

    uint16_t Socket::dst_port()
    {
        return internal->pcb->local_port;
    }
}
