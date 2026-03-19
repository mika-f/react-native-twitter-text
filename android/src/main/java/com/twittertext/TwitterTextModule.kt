package com.twittertext

import com.facebook.react.bridge.ReactApplicationContext

class TwitterTextModule(reactContext: ReactApplicationContext) :
  NativeTwitterTextSpec(reactContext) {

  override fun multiply(a: Double, b: Double): Double {
    return a * b
  }

  companion object {
    const val NAME = NativeTwitterTextSpec.NAME
  }
}
