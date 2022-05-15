#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`
#include<iostream>
template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity), _mp({}), _nidx(0), _eof(false), _eidx(0),_unassembled_bytes(0) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if(eof){
        _eidx = index + data.size();
        _eof = true;
    }
    size_t idx = index;
    if(idx <= _nidx){
        if(data.size() > _nidx - idx){
            _nidx +=_output.write(data.substr(_nidx - idx));
            auto iter = _mp.begin();
            while(iter != _mp.end() && iter->first <= _nidx){
                if(iter->second.size() > _nidx - iter->first){
                    _nidx += _output.write(iter->second.substr(_nidx - iter->first));
                }
                _unassembled_bytes -= iter->second.size();
                _mp.erase(iter);
                iter = _mp.begin();
            }
        }
    }
    else{
        string str = data;
        auto inext = _mp.lower_bound(idx);
        // if inext's begin isn't bigger than data's end, 
        // merge inext into data and erase inext
        while(inext != _mp.end() && inext->first <= idx + str.size()){
            if(inext->first + inext->second.size() > idx + str.size()){
                str += inext->second.substr(idx + str.size() - inext->first);
            }
            _unassembled_bytes -= inext->second.size();
            _mp.erase(inext);
            inext = _mp.lower_bound(idx);
        }
        auto ilast = _mp.lower_bound(idx);
        if(!_mp.empty() && ilast != _mp.begin()){
            // if ilast's end isn't less than data's begin,
            // merge ilast into data and erase ilast
            ilast--;
            if(ilast->first + ilast->second.size() >= idx){
                if(ilast->first + ilast->second.size() < index + str.size()){
                    str = ilast->second + str.substr(ilast->first + ilast->second.size() - idx);
                }
                else{
                    str = ilast->second;
                }
                _unassembled_bytes -= ilast->second.size();
                idx = ilast->first;
                _mp.erase(ilast);
            }
        }
        size_t store_size = min(_capacity - _unassembled_bytes - _output.buffer_size(),str.size());
        _unassembled_bytes += store_size;
        _mp.emplace(idx,str.substr(0,store_size));
    }

    if(_eof && _nidx >= _eidx)
        _output.end_input();
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

bool StreamReassembler::empty() const { return _unassembled_bytes == 0; }
