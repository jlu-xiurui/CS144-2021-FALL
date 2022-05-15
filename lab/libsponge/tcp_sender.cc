#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;
void Timer::tick_handle(const size_t ticks,const uint16_t window_size,std::queue<TCPSegment>& segments_out){
    if(_running){
        if(_RTO <= ticks){
            segments_out.push(_outstandings.front().first);
            if(window_size != 0){
                _consecutive_retransmissions++;
                _base_RTO *= 2;
            }
            _RTO = _base_RTO;
        }
        else
            _RTO -= ticks;
    }
}

uint64_t Timer::ack_handle(const uint64_t ackno){
    uint64_t ret = 0;
    _consecutive_retransmissions = 0;
    while(!_outstandings.empty()){
        uint64_t end_seqno = _outstandings.front().second;
        if(end_seqno > ackno)
            break;
        _RTO = _initial_RTO;
        _base_RTO = _initial_RTO;
        ret += _outstandings.front().first.length_in_sequence_space();
        _outstandings.pop();
    }
    if(_outstandings.empty())
        _running = false;
    return ret;
}
//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _timer(retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    uint64_t window_size = _window_size > 0 ? static_cast<uint64_t>(_window_size) : 1ul;
    uint64_t end_seqno = window_size + _ackno;
    while(!_fin_sended && _next_seqno < end_seqno && (!_stream.buffer_empty() || _next_seqno == 0 || _stream.eof())){
        _timer.on();
        TCPSegment seg;
        seg.header().syn = _next_seqno == 0;
        seg.header().seqno = wrap(_next_seqno,_isn);
        size_t read_len = min(static_cast<size_t>(end_seqno - _next_seqno) - seg.header().syn,_stream.buffer_size());
        read_len = min(read_len, TCPConfig::MAX_PAYLOAD_SIZE);
        Buffer payload(_stream.read(read_len));
        seg.payload() = payload;
        size_t seg_len = seg.length_in_sequence_space();
        if(seg_len < end_seqno - _next_seqno){
            seg.header().fin = _stream.eof();
            seg_len += _stream.eof();
            _fin_sended = _stream.eof();
        }
        _bytes_in_flight += seg_len;
        _next_seqno += seg_len;
        _segments_out.push(seg);
        _timer.push_outstanding(seg,_next_seqno);
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t tmp = unwrap(ackno,_isn,_next_seqno);
    if(tmp <= _next_seqno){
        _ackno = tmp; 
        _window_size = window_size;
        _bytes_in_flight -= _timer.ack_handle(_ackno);
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _timer.tick_handle(ms_since_last_tick,_window_size,_segments_out);
}

unsigned int TCPSender::consecutive_retransmissions() const { return _timer.consecutive_retransmissions(); }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = wrap(_next_seqno,_isn);
    _segments_out.push(seg);

}
