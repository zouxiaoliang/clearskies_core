/*
 *  This file is part of clearskies_core file synchronization program
 *  Copyright (C) 2014 Pedro Larroy

 *  clearskies_core is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.

 *  clearskies_core is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.

 *  You should have received a copy of the GNU Lesser General Public License
 *  along with clearskies_core.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include "int_types.h"
#include "message.hpp"
#include "messagecoder.hpp"
#include <stddef.h>


namespace cs
{
namespace protocol
{

inline bool has_signature(const char c)
{
    return c == '$' || c == 's';
}


inline bool has_payload(const char c)
{
    return c == '$' || c == '!';
}


struct MsgRstate
{
    MsgRstate():
          prefix()
        , msg_len()
        , found(false)
        , garbage(false)
        , encoded()
        , encoded_sz()
        , signature()
        , signature_sz()
        , end()
    {}

    MsgRstate& set_garbage()
    {
        garbage = true;
        return *this;
    }

    bool payload() const
    {
        return has_payload(prefix);
    }

    bool has_signature() const
    {
        return signature_sz != 0;
    }

    /// prefix !: payload s: signed $: signed payload
    char prefix;
    size_t msg_len;
    bool found;
    bool garbage;
    bool too_big;
    const char* encoded;
    size_t encoded_sz;
    const char* signature;
    size_t signature_sz;
    /// pos where msg ends, data is processed and destroyed until this pos
    size_t end;
};


/// @return where the message starts and its components in the input buffer
MsgRstate find_message(const std::string& buff);


struct PayLoadFound
{
    PayLoadFound():
        found()
        , garbage()
        , size_plus_newline_sz()
        , data_sz()
    {}

    void reset()
    {
        found = false;
        garbage = false;
        size_plus_newline_sz = 0;
        data_sz = 0;
    }

    PayLoadFound& set_garbage()
    {
        garbage = true;
        return *this;
    }

    bool error() const
    {
        return garbage || too_big;
    }

    explicit operator bool() const
    {
        return found;
    }
    size_t total_size() const
    {
        return size_plus_newline_sz + data_sz;
    }
    bool found;
    bool garbage;
    bool too_big;
    /// size field + newline
    size_t size_plus_newline_sz;
    size_t data_sz;
};

/// @return info about a payload chunk on the input buffer
PayLoadFound find_payload(const std::string& buff);

/**
 * @brief Base protocol state class for all protocols
 * @author plarroy
 *
 * Low level buffer IO handling
 *
 * Input data is fed and when messages are complete, handle_message is called which implementes the
 * message dispatching logic in derived classes
 */
class ProtocolState
{
public:
    /// type of callback for writing data
    typedef std::function<void(const char*, size_t)> do_write_t;


    static size_t s_msg_preamble_max;
    static size_t s_msg_signature_max;
    static size_t s_msg_size_max;
    static size_t s_payload_chunk_size_max;
    /// initial size of the input buffer
    static size_t s_input_buff_size;
    ProtocolState():
          m_input_buff()
        , m_output_buff()
        , m_last_has_payload()
        , m_payload_ended(true)
        , m_read_payload(false)
        , m_pl_found()
        , m_msg_coder()
        , m_do_write([](const char*, size_t) { assert(false); })
        , m_write_in_progress(false)
    {
        m_input_buff.reserve(s_input_buff_size);
    }

    virtual ~ProtocolState() = default;
    ProtocolState(const ProtocolState&) = delete;
    ProtocolState& operator=(const ProtocolState&) = delete;
    ProtocolState(ProtocolState&&) = default;
    ProtocolState& operator=(ProtocolState&&) = default;

    void input(const std::string& s)
    {
        input(s.c_str(), s.size());
    }
    /**
     * feed input data, for example from socket IO
     * Once a full message is read, handle_message is called
     *
     * to be called by the event library on read
     */
    void input(const char* data, size_t len);

    void send_message(const message::Message&);
    void send_payload_chunk(std::string&& chunk);

    void set_write_fun(do_write_t do_write)
    {
        m_do_write = do_write;
    }


    /// to be called by the event library on write when the last write finished
    void on_write_finished();

    /**
     * will write the next output buffer by calling the write function @sa m_do_write
     * @post m_write_in_progress will be true
     */
    void write_next_buff();

    /// called by on_write_finished to signal that we are out of data (ex. send more manifest
    /// messages, or send the next chunk of payload)
    virtual void handle_empty_output_buff() {}

    /// called when a message is completely read on the input buffer
    virtual void handle_message(std::unique_ptr<message::Message>) = 0;
    /// called after a message with the payload flag was handled and payload was input
    virtual void handle_payload(const char* data, size_t len) = 0;
    /// called at the end of payload (record of size 0)
    virtual void handle_payload_end() = 0;
    /// in case of garbage we should probably close the connection ASAP
    virtual void handle_msg_garbage(const std::string& buff) {};
    /// garbage in payload
    virtual void handle_pl_garbage(const std::string& buff) {};

private:
    /// internal input buffer accumulating data until it can be processed
    std::string m_input_buff;
    /// queue of buffers to write, we write from front to back, new appended to back, when wrote,
    /// removed from front.
    std::deque<std::string> m_output_buff;

    bool m_last_has_payload;
    bool m_payload_ended;

    /// true if we are reading a payload section, false if we are reading or expecting a message
    bool m_read_payload;
    PayLoadFound m_pl_found;

    /// encoder used for message data
    message::Coder m_msg_coder;

public:
    /// callback used to write data
    do_write_t m_do_write;
    bool m_write_in_progress;
};
} // end ns
} // end ns
