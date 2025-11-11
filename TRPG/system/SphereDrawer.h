#pragma once

#include	"CommonTypes.h"
#include	"transform.h"

void SphereDrawerInit();
void SphereDrawerDraw(float radius, Color col, float ex, float ey, float ez);
void SphereDrawerDraw(SRT rts, Color col);
void SphereDrawerDraw(Matrix4x4 mtx, Color col);

