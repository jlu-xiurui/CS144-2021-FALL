#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : _end_input(false), _read_bytes(0), _write_bytes(0), _buf(capacity+1,'0') ,_head(0) , _tail(0), _sz(0), _capacity(capacity) ,_error(false){
}

size_t ByteStream::write(const string &data) {
    size_t idx = 0;
    size_t data_sz = data.size();
    while((_tail != _head || _sz != _capacity) && data_sz != idx){
       _buf[_tail] = data[idx++];
       _tail = (_tail + 1) % _capacity;
       _sz++;
    }
    _write_bytes += idx;
    return idx;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    string ret;
    size_t peek_len = min(len, _sz);
    size_t idx = _head;
    while(peek_len--){
        ret += _buf[idx];
        idx = (idx + 1) % _capacity;
    }
    return ret;
}

//! \param[in] len bytes will be removed from the output side of the buffevoid ByteStream::pop_output(const size_t len) { DUMMY_CODE(len); }
void ByteStream::pop_output(const size_t len) {
    size_t sz = min(len,_sz);
    _head = (_head + sz) % _capacity;
    _sz -= sz;
    _read_bytes += sz;
}
//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string ret;
    size_t sz = min(len,_sz);
    size_t read_len = 0;
    while(read_len++ != sz){
        ret += _buf[_head];
        _head = (_head + 1) % _capacity;
    }
    _sz -= sz;
    _read_bytes += ret.size();
    return ret;
}

void ByteStream::end_input() {
    _end_input = true;
    return;
}

bool ByteStream::input_ended() const { return _end_input; }

size_t ByteStream::buffer_size() const { return _sz; }

bool ByteStream::buffer_empty() const { return _sz == 0; }

bool ByteStream::eof() const { return _end_input && _sz == 0; }

size_t ByteStream::bytes_written() const { return _write_bytes; }

size_t ByteStream::bytes_read() const { return _read_bytes; }

size_t ByteStream::remaining_capacity() const { return _capacity - _sz; }
