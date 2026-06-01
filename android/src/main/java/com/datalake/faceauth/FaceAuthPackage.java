/**
 * FaceAuthPackage.java
 * * Datalake 3.0 — React Native Package Registration (Android)
 * Hackathon 7.0 | NHAI
 * *
 * Registers the FaceAuthModule with React Native's module registry.
 * This package must be added to the host app's getPackages() list
 * in MainApplication.java:
 *
 *   @Override
 *   protected List<ReactPackage> getPackages() {
 *       List<ReactPackage> packages = new PackageList(this).getPackages();
 *       packages.add(new FaceAuthPackage());
 *       return packages;
 *   }
 */

package com.datalake.faceauth;

import androidx.annotation.NonNull;

import com.facebook.react.ReactPackage;
import com.facebook.react.bridge.NativeModule;
import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.uimanager.ViewManager;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public class FaceAuthPackage implements ReactPackage {

    @Override
    @NonNull
    public List<NativeModule> createNativeModules(
            @NonNull ReactApplicationContext reactContext) {

        List<NativeModule> modules = new ArrayList<>();
        modules.add(new FaceAuthModule(reactContext));
        return modules;
    }

    @Override
    @NonNull
    public List<ViewManager> createViewManagers(
            @NonNull ReactApplicationContext reactContext) {

        // This module has no custom views
        return Collections.emptyList();
    }
}
