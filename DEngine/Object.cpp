#include "pch.h"
#include "Object.h"

Object::Object(OBJECT_TYPE type) : _objectType(type)
{
	static int32 idGenerator = 1;
	_id = idGenerator;
	idGenerator++;
}

Object::~Object()
{

}