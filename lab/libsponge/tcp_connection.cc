#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;
void TCPConnection::send_rst(){
    while(!_segments_out.empty()){
        _segments_out.pop();
    }
    _sender.send_empty_segment();
    TCPSegment seg = _sender.segments_out().back();
    seg.header().rst = true;
    _segments_out.push(seg);
    _receiver.stream_out().set_error();
    _sender.stream_in().set_error();
    _active = false;
}
void TCPConnection::try_clean_closed(){
    if(_receiver.stream_out().eof() && _sender.stream_in().eof() &&  _sender.bytes_in_flight() == 0){
        if(_linger_after_streams_finish == false || _linger_time >= _cfg.rt_timeout * 10){
            _active = false;
        }
    }
}
void TCPConnection::update_queue(){
    while(!_sender.segments_out().empty()){
        TCPSegment seg = _sender.segments_out().front();
        if(_receiver.ackno().has_value()){
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
            seg.header().win = _receiver.window_size();
        }
        _segments_out.push(seg);
        _sender.segments_out().pop();
    }
}
size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _linger_time; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    _linger_time = 0;
    if(seg.header().rst){
        _receiver.stream_out().set_error();
        _sender.stream_in().set_error();
        _active = false;
        return;
    }
    _receiver.segment_received(seg);
    if(seg.header().ack){
        _sender.ack_received(seg.header().ackno,seg.header().win);
        if(_sender.next_seqno_absolute() != 0){
            _sender.fill_window();
            update_queue();
        }
    }
    if(_receiver.stream_out().eof() && !_sender.stream_in().eof())
        _linger_after_streams_finish = false;
    try_clean_closed(); 
    if(seg.length_in_sequence_space() != 0 && _receiver.ackno().has_value()){
        if(_sender.next_seqno_absolute() == 0 && seg.header().syn == 1)
            _sender.fill_window();
        else
            _sender.send_empty_segment();
        update_queue();
    }
    if(seg.length_in_sequence_space() == 0 && _receiver.ackno().has_value() && seg.header().seqno== _receiver.ackno().value() - 1){
        _sender.send_empty_segment();
        update_queue();
    }
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    size_t ret = _sender.stream_in().write(data);
    _sender.fill_window();
    update_queue();
    return ret;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _sender.tick(ms_since_last_tick);
    update_queue();
    _linger_time += ms_since_last_tick;
    if(_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS){
        send_rst();
        return;
    }
    try_clean_closed();
}

void TCPConnection::end_input_stream() { 
    _sender.stream_in().end_input();
    _sender.fill_window();
    update_queue();
}

void TCPConnection::connect() {
    _sender.fill_window();
    update_queue();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            send_rst();
            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
