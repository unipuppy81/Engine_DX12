#pragma once
#include "MonoBehaviour.h"

class TestDirectionalLight : public MonoBehaviour
{
public:
    virtual void Update() override;

private:
    float _time = 0.f;
};

