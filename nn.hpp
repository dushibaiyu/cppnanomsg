/*
    Copyright (c) 2013 250bpm s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom
    the Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
    IN THE SOFTWARE.
*/

#ifndef NN_HPP_INCLUDED
#define NN_HPP_INCLUDED

#include <nanomsg/nn.h>
#include <nanomsg/tcp.h>
#include <nanomsg/pubsub.h>
#include <nanomsg/survey.h>
#include <nanomsg/pair.h>
#include <nanomsg/reqrep.h>
#include <nanomsg/pipeline.h>

#include <cassert>
#include <cstring>
#include <algorithm>
#include <exception>
#include <string>


#if defined __GNUC__
#define nn_slow(x) __builtin_expect ((x), 0)
#else
#define nn_slow(x) (x)
#endif

namespace nn
{

class exception : public std::exception
{
public:

    exception () : err (nn_errno ()) {}

    virtual const char *what () const throw ()
    {
        return nn_strerror (err);
    }

    int num () const
    {
        return err;
    }

private:

    int err;
};

inline const char *symbol (int i, int *value)
{
    return nn_symbol (i, value);
}

inline void *allocmsg (size_t size, int type)
{
    void *msg = nn_allocmsg (size, type);
    if (nn_slow (!msg))
        throw nn::exception ();
    return msg;
}

inline int freemsg (void *msg)
{
    int rc = nn_freemsg (msg);
    if (nn_slow (rc != 0))
        throw nn::exception ();
    return rc;
}

class socket
{
public:

    inline socket (int domain, int protocol)
    {
        s = nn_socket (domain, protocol);
        if (nn_slow (s < 0))
            throw nn::exception ();
    }

    inline int fd() const {return s;}

    inline ~socket ()
    {
        int rc = nn_close (s);
        assert (rc == 0);
    }

    inline void setsockopt (int level, int option, const void *optval,size_t optvallen)
    {
        int rc = nn_setsockopt (s, level, option, optval, optvallen);
        if (nn_slow (rc != 0))
            throw nn::exception ();
    }

    inline void getsockopt (int level, int option, void *optval,size_t *optvallen) const
    {
        int rc = nn_getsockopt (s, level, option, optval, optvallen);
        if (nn_slow (rc != 0))
            throw nn::exception ();
    }


    inline int bind (const char *addr)
    {
        int rc = nn_bind (s, addr);
        if (nn_slow (rc < 0))
            throw nn::exception ();
        return rc;
    }

    inline int connect (const char *addr)
    {
        int rc = nn_connect (s, addr);
        if (nn_slow (rc < 0))
            throw nn::exception ();
        return rc;
    }

    inline void shutdown (int how)
    {
        int rc = nn_shutdown (s, how);
        if (nn_slow (rc != 0))
            throw nn::exception ();
    }

    inline int send (const void *buf, size_t len, int flags = 0)
    {
        int rc = nn_send (s, buf, len, flags);
        if (nn_slow (rc < 0)) {
            if (nn_slow (nn_errno () != EAGAIN))
                throw nn::exception ();
            return -1;
        }
        return rc;
    }

    inline int recv (void *buf, size_t len, int flags = 0)
    {
        int rc = nn_recv (s, buf, len, flags);
        if (nn_slow (rc < 0)) {
            if (nn_slow (nn_errno () != EAGAIN))
                throw nn::exception ();
            return -1;
        }
        return rc;
    }

    // Note: free use freemsg(ret.iov_base)
    inline nn_iovec recv (int flags = 0)
    {
        void * buf = nullptr;
        int rc = nn_recv (s, buf, NN_MSG, flags);
        if (nn_slow (rc < 0)) {
            if (nn_slow (nn_errno () != EAGAIN))
                throw nn::exception ();
            return nn_iovec{nullptr,0};
        }
        return nn_iovec{buf,static_cast<size_t>(rc)};
    }

    inline int sendmsg (const struct nn_msghdr *msghdr, int flags = 0)
    {
        int rc = nn_sendmsg (s, msghdr, flags);
        if (nn_slow (rc < 0)) {
            if (nn_slow (nn_errno () != EAGAIN))
                throw nn::exception ();
            return -1;
        }
        return rc;
    }

    inline int recvmsg (struct nn_msghdr *msghdr, int flags = 0)
    {
        int rc = nn_recvmsg (s, msghdr, flags);
        if (nn_slow (rc < 0)) {
            if (nn_slow (nn_errno () != EAGAIN))
                throw nn::exception ();
            return -1;
        }
        return rc;
    }

private:

    int s;

    /*  Prevent making copies of the socket by accident. */
    socket (const socket&) = delete;
    void operator = (const socket&);
};

inline void term ()
{
    nn_term ();
}

// tcp
inline bool gettcpnodelay(const socket &s){
    int v = 0;
    size_t len = sizeof(v);
    s.getsockopt(NN_SOL_SOCKET, NN_TCP_NODELAY,&v,&len);
    return v;
}

inline void settcpnodelay(socket &s, bool enable){
    int v = enable;
    s.setsockopt(NN_SOL_SOCKET, NN_TCP_NODELAY,&v,sizeof(v));
}

// req -rep  time : ms
inline int  getresendinterval(const socket &s)
{
    int v = 0;
    size_t len = sizeof(v);
    s.getsockopt(NN_REQ, NN_REQ_RESEND_IVL,&v,&len);
    return v;
}

inline void setresendinterval(socket &s, int time)
{
    s.setsockopt(NN_REQ, NN_REQ_RESEND_IVL, &time, sizeof(time));
}


// sub pub
inline void subscribe(socket &s, const void *topic, size_t topiclen)
{ s.setsockopt(NN_SUB, NN_SUB_SUBSCRIBE, topic, topiclen); }

inline void subscribe(socket &s, const char *topic)
{ subscribe(s, topic, std::strlen(topic)); }

inline void subscribe(socket &s, const std::string &topic)
{ subscribe(s, topic.data(), topic.size()); }

inline void subscribe(socket &s)
{ subscribe(s, "", 0); }

inline void unsubscribe(socket &s, const void *topic, size_t topiclen)
{ s.setsockopt(NN_PUB, NN_SUB_UNSUBSCRIBE, topic, topiclen); }

inline void unsubscribe(socket &s, const char *topic)
{ unsubscribe(s, topic, std::strlen(topic)); }

inline void unsubscribe(socket &s, const std::string &topic)
{ unsubscribe(s, topic.data(), topic.size()); }

inline void unsubscribe(socket &s)
{ unsubscribe(s, "", 0); }
// time out
// ms
inline int getlinger(const socket &s)
{
    int v = 0;
    size_t len = sizeof(v);
    s.getsockopt(NN_SOL_SOCKET, NN_LINGER,&v,&len);
    return v;
}

inline int getrecvtimeout(const socket &s)
{
    int v = 0;
    size_t len = sizeof(v);
    s.getsockopt(NN_SOL_SOCKET, NN_RCVTIMEO,&v,&len);
    return v;
}

inline int getsendtimeout(const socket &s)
{
    int v = 0;
    size_t len = sizeof(v);
    s.getsockopt(NN_SOL_SOCKET, NN_SNDTIMEO,&v,&len);
    return v;
}

inline void setlinger(socket &s, int t)
{
    s.setsockopt(NN_SOL_SOCKET, NN_LINGER,&t,sizeof(t));
}

inline void setrecvtimeout(socket &s, int t)
{ s.setsockopt(NN_SOL_SOCKET, NN_RCVTIMEO,&t,sizeof(t)); }

inline void setsendtimeout(socket &s, int t)
{ s.setsockopt(NN_SOL_SOCKET, NN_SNDTIMEO,&t,sizeof(t));}

// surveyor
inline int  getsurveyordeadline(const socket &s)
{
    int v = 0;
    size_t len = sizeof(v);
    s.getsockopt(NN_SURVEYOR, NN_SURVEYOR_DEADLINE,&v,&len);
    return v;
}

inline void setsurveyordeadline(socket &s, int t)
{
    s.setsockopt(NN_SURVEYOR, NN_SURVEYOR_DEADLINE,&t,sizeof(t));
}

// poll

inline nn_pollfd  pollfd(socket &s,bool r = true,bool w = false){
    nn_pollfd ret{0};
    ret.fd = s.fd();
    if(r) ret.events |= NN_POLLIN;
    if(w) ret.events |= NN_POLLOUT;
    return ret;
}

inline int poll(nn_pollfd * pfd,int size,int timeOut) {
    int rc = nn_poll(pfd,size,timeOut);
    if (nn_slow (rc < 0))
        throw nn::exception ();
    return rc;
}
// return 0 ,| NN_POLLIN, | NN_POLLOUT
inline int poll(socket &s,int timeOut,bool r = true, bool w = false) {
    nn_pollfd pfd = pollfd(s,r,w);
    int rc = poll(&pfd,1,timeOut);
    if(rc > 0){
        return pfd.revents;
    }
    return 0;
}
// device
inline int device(socket &s1, socket &s2)
{
    return nn_device(s1.fd(), s2.fd());
}

}

#undef nn_slow

#endif


