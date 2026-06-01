# # FaceAuth.podspec — iOS CocoaPods Specification
# Datalake 3.0 Face Authentication Native Module
# Hackathon 7.0 | NHAI

# Builds the shared C++ engine for iOS with:
#   - C++17 standard
#   - Accelerate framework (vImage for fast YUV→RGB conversion)
#   - System sqlite3 library
#   - Embedded nanopb for compact Protobuf serialization
#   - Placeholder for ONNX Runtime xcframework
#
# INSTALLATION:
#   Add to the host app's Podfile:
#     pod 'FaceAuth', :path => '../node_modules/datalake3-faceauth/ios'
# Pod::Spec.new do |s|
  s.name         = "FaceAuth"
  s.version      = "1.0.0"
  s.summary      = "Offline Face Recognition & Active Liveness Detection"
  s.description  = <<-DESC
    Production-grade native module for offline facial recognition and
    active liveness detection, built for the Datalake 3.0 React Native
    application. Features zero-allocation frame pipeline, INT8 quantized
    MobileFaceNet inference, and cryptographic sync/purge system.
  DESC
  s.homepage     = "https://github.com/datalake/faceauth"
  s.license      = { :type => "MIT", :file => "../LICENSE" }
  s.author       = { "NHAI Hackathon Team" => "team@datalake.in" }

  # Platform
  s.platforms    = { :ios => "12.0" }
  s.source       = { :git => "https://github.com/datalake/faceauth.git",
                     :tag => s.version.to_s }

  # Source Files
  # Include iOS-specific ObjC++ and all shared C++ code.
  # EXCLUDE the Android-specific platform handler.
  s.source_files = [
    "FaceAuth/**/*.{h,m,mm,cpp}",
    "../cpp/**/*.{h,hpp,cpp,c}"
  ]
  s.exclude_files = [
    "../cpp/PlatformImageHandler_Android.cpp"
  ]

  # Compiler Configuration
  s.pod_target_xcconfig = {
    # C++17 required for structured bindings, std::optional, etc.
    "CLANG_CXX_LANGUAGE_STANDARD" => "c++17",
    "CLANG_CXX_LIBRARY"          => "libc++",

    # Include paths for shared C++ headers
    "HEADER_SEARCH_PATHS" => [
      "\"$(PODS_TARGET_SRCROOT)/../cpp\"",
      "\"$(PODS_TARGET_SRCROOT)/../cpp/third_party\"",
      "\"$(PODS_TARGET_SRCROOT)/../cpp/third_party/nanopb\"",
      "\"$(PODS_TARGET_SRCROOT)/../cpp/proto\""
    ].join(" "),

    # Link system SQLite (iOS includes SQLite by default)
    "OTHER_LDFLAGS" => "-lsqlite3",

    # Match Android flags where applicable
    "GCC_OPTIMIZATION_LEVEL" => "2",
    "OTHER_CPLUSPLUSFLAGS" => "-frtti -fno-exceptions"
  }

  # Frameworks
  # Accelerate: provides vImage for NEON-accelerated YUV→RGB conversion
  s.frameworks = ["Accelerate"]

  # React Native Dependencies
  s.dependency "React-Core"
  s.dependency "React-callinvoker"
  s.dependency "RCT-Folly"

  # ONNX Runtime (Placeholder)
  # Uncomment when the ONNX Runtime iOS xcframework is downloaded.
  # Download script: scripts/download_onnxruntime_ios.sh
  #
  # s.vendored_frameworks = "libs/onnxruntime.xcframework"

  # Compiler Flags
  # Force Objective-C++ compilation for .mm files
  s.compiler_flags = '-x objective-c++'
end
