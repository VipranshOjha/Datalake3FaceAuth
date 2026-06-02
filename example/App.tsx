import React, { useEffect, useState } from 'react';
import { StyleSheet, View, Text, SafeAreaView } from 'react-native';
import { Camera, useCameraDevice, useFrameProcessor } from 'react-native-vision-camera';
import FaceAuth from 'datalake3-faceauth';

export default function App() {
  const [hasPermission, setHasPermission] = useState(false);
  const device = useCameraDevice('front');

  useEffect(() => {
    (async () => {
      const status = await Camera.requestCameraPermission();
      setHasPermission(status === 'granted');
    })();
  }, []);

  // Frame processor for facial verification bindings test
  const frameProcessor = useFrameProcessor((frame) => {
    'worklet';
    // Example invocation for your native module
    // FaceAuth.processFrame(frame); 
  }, []);

  if (!hasPermission) {
    return (
      <SafeAreaView style={styles.container}>
        <Text style={styles.text}>Awaiting Camera Permission...</Text>
      </SafeAreaView>
    );
  }

  if (device == null) {
    return (
      <SafeAreaView style={styles.container}>
        <Text style={styles.text}>No Camera Device Found</Text>
      </SafeAreaView>
    );
  }

  return (
    <SafeAreaView style={styles.container}>
      <Camera
        style={StyleSheet.absoluteFill}
        device={device}
        isActive={true}
        frameProcessor={frameProcessor}
      />
      <View style={styles.overlay}>
         <Text style={styles.text}>Face Verification Target Layout</Text>
         <View style={styles.targetBox} />
      </View>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: 'black', justifyContent: 'center', alignItems: 'center' },
  overlay: { ...StyleSheet.absoluteFillObject, justifyContent: 'center', alignItems: 'center' },
  text: { color: 'white', fontSize: 18, marginBottom: 20, backgroundColor: 'rgba(0,0,0,0.5)', padding: 5 },
  targetBox: { width: 250, height: 250, borderWidth: 3, borderColor: '#00FF00', borderRadius: 12 }
});
