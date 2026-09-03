package com.selfso.robohero

import android.app.Application

class RoboHeroApp : Application() {
    override fun onCreate() {
        super.onCreate()
        instance = this
    }

    companion object {
        lateinit var instance: RoboHeroApp
            private set
    }
}
