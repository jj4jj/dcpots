#pragma once
#include "noncopyable.h"

//not a thread safe
template <class T>
struct singleton : public noncopyable {
	static T & instance() {
		static T sto;
		return sto;
	}
    static T & Instance() {return instance();}
};