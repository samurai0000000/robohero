/*
 * Main.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <Arduino.h>
#include "RoboHeroApp.hxx"

static RoboHeroApp g_app;

void setup()
{
    g_app.setup();
}

void loop()
{
    g_app.loop();
}

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
