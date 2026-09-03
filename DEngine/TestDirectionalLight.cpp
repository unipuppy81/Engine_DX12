#include "pch.h"
#include "TestDirectionalLight.h"
#include "GameObject.h"
#include "Light.h"
#include "Timer.h"

void TestDirectionalLight::Update()
{
    _time += GET_SINGLE(Timer)->GetDeltaTime();

    float angle = sinf(_time * 0.4f) * XMConvertToRadians(45.f);

    float baseX = 0.4f;
    float baseZ = 0.3f;

    float x = baseX * cosf(angle) - baseZ * sinf(angle);
    float z = baseX * sinf(angle) + baseZ * cosf(angle);

    Vec3 direction(x, -1.f, z);

    GetGameObject()->GetLight()->SetLightDirection(direction);
}