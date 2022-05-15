#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    TCPHeader header = seg.header();
    if(header.syn){
        _syn_arrived = true;
        _isn = header.seqno;
    }
    if(_syn_arrived){
        bool eof = header.fin;
        uint64_t idx = unwrap(header.seqno,_isn,_ackno);
        if((idx == 0 && !header.syn) || idx >= window_size() + _ackno)
            return;
        _reassembler.push_substring(seg.payload().copy(),idx - 1 + header.syn,eof);
        _ackno = _reassembler.next_idx() + _syn_arrived + _reassembler.stream_out().input_ended();
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    if(!_syn_arrived)
        return {};
    return wrap(_ackno,_isn);
}

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size();}
