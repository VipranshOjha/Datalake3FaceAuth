/**
 * FaceAuthModule.ts
 * * Datalake 3.0 — TypeScript JSI Wrapper
 * Hackathon 7.0 | NHAI
 * *
 * Type-safe TypeScript interface for the native FaceAuth JSI module.
 * Wraps the raw global.__FaceAuthModule HostObject with proper
 * TypeScript interfaces, error handling, and JSDoc documentation.
 *
 * USAGE:
 *   import { FaceAuth } from './native/FaceAuthModule';
 *
 *   // Initialize (once, at app startup)
 *   FaceAuth.install();
 *   FaceAuth.initializeModels('/path/to/models');
 *
 *   // Verify a user
 *   const result = await FaceAuth.nativeVerifyUser('user-123');
 *   console.log(result.success, result.confidence);
 */

import { NativeModules } from 'react-native';

// // Type Definitions
// /** Result of a complete face verification pipeline execution. */
export interface VerificationResult {
  /** Whether the user was successfully verified. */
  success: boolean;
  /** Cosine similarity confidence score (0.0 – 1.0). */
  confidence: number;
  /** The user ID that was matched against the enrolled cache. */
  userId: string;
  /** Liveness check status: "PASSED", "FAILED_BLINK", "FAILED_SMILE", "TIMEOUT", "SPOOF_DETECTED", "NO_FACE". */
  livenessStatus: string;
  /** Monotonic clock timestamp (milliseconds since boot). */
  timestampMonotonic: number;
  /** GPS latitude at time of verification. */
  latitude: number;
  /** GPS longitude at time of verification. */
  longitude: number;
}

/** Health check status of the native module. */
export interface ModuleStatus {
  /** Whether ONNX model sessions are loaded and ready. */
  modelsLoaded: boolean;
  /** Number of free slots in the circular frame buffer (0–5). */
  bufferSlotsFree: number;
  /** NTP clock offset in milliseconds (0 if never synced). */
  clockOffsetMs: number;
  /** Number of attendance records pending sync to AWS. */
  pendingSyncCount: number;
  /** Number of enrolled users in the local embedding cache. */
  enrolledUserCount: number;
  /** Whether the NativeWorker background thread is alive. */
  workerAlive: boolean;
}

/** Liveness challenge sequence issued to the user. */
export interface LivenessChallengeSequence {
  /** Ordered list of challenges, e.g., ["BLINK", "SMILE"]. Randomized per session. */
  challenges: string[];
  /** Monotonic timestamp when the challenge was issued (ms). */
  issuedAtMonotonic: number;
  /** Timeout per challenge in milliseconds (default: 10000). */
  timeoutMs: number;
}

/** Raw native module interface exposed at global.__FaceAuthModule. */
interface FaceAuthNativeModule {
  nativeVerifyUser(userId: string): Promise<VerificationResult>;
  initializeModels(modelDir: string): boolean;
  getModuleStatus(): ModuleStatus;
  startLivenessChallenge(): LivenessChallengeSequence;
  enrollUser(userId: string, featureData: ArrayBuffer): boolean;
  setSyncEndpoint(url: string): void;
}

// // Module Access
// /**
 * Get the native FaceAuth JSI module from the global scope.
 * Returns null if install() hasn't been called yet.
 */
function getNativeModule(): FaceAuthNativeModule | null {
  // @ts-ignore — global.__FaceAuthModule is set by C++ via JSI
  return (global as any).__FaceAuthModule ?? null;
}

/**
 * Get the native module, throwing if it's not installed.
 */
function requireNativeModule(): FaceAuthNativeModule {
  const mod = getNativeModule();
  if (!mod) {
    throw new Error(
      '[FaceAuth] Native module not installed. ' +
      'Call FaceAuth.install() first. ' +
      'Ensure libdatalake_faceauth.so is linked on Android ' +
      'and FaceAuth pod is installed on iOS.'
    );
  }
  return mod;
}

// // Public API
// export const FaceAuth = {
  /**
   * Install the native JSI module. Must be called once at app startup,
   * AFTER the React Native bridge is initialized.
   *
   * @returns true if installation succeeded, false otherwise.
   *
   * @example
   * ```ts
   * // In App.tsx or index.js
   * import { FaceAuth } from './native/FaceAuthModule';
   * FaceAuth.install();
   * ```
   */
  install(): boolean {
    try {
      const result = NativeModules.FaceAuthModule?.install();
      if (result) {
        console.log('[FaceAuth] JSI module installed successfully');
      } else {
        console.error('[FaceAuth] install() returned false');
      }
      return !!result;
    } catch (error) {
      console.error('[FaceAuth] install() threw:', error);
      return false;
    }
  },

  /**
   * Initialize ONNX model sessions for face detection, landmark
   * extraction, and face recognition.
   *
   * @param modelDir - Absolute path to the directory containing
   *                   blazeface_int8.onnx, landmark68_int8.onnx,
   *                   and mobilefacenet_int8.onnx
   * @returns true if all models loaded successfully.
   *
   * @example
   * ```ts
   * const modelPath = Platform.select({
   *   android: RNFS.DocumentDirectoryPath + '/models',
   *   ios: RNFS.MainBundlePath + '/models',
   * });
   * FaceAuth.initializeModels(modelPath);
   * ```
   */
  initializeModels(modelDir: string): boolean {
    return requireNativeModule().initializeModels(modelDir);
  },

  /**
   * Run the complete face verification pipeline:
   * Detection → Landmarks → Liveness → Recognition → Match.
   *
   * Executes asynchronously on the high-priority background thread.
   * The JS thread remains unblocked during inference.
   *
   * @param userId - The user ID to verify against enrolled embeddings.
   * @returns Promise resolving to a VerificationResult.
   *
   * @example
   * ```ts
   * const result = await FaceAuth.nativeVerifyUser('user-123');
   * if (result.success && result.confidence > 0.6) {
   *   console.log('Verified!', result.userId);
   * }
   * ```
   */
  nativeVerifyUser(userId: string): Promise<VerificationResult> {
    return requireNativeModule().nativeVerifyUser(userId);
  },

  /**
   * Get the current health status of the native module.
   *
   * @returns ModuleStatus snapshot.
   */
  getModuleStatus(): ModuleStatus {
    return requireNativeModule().getModuleStatus();
  },

  /**
   * Start a new liveness challenge session.
   * Returns a randomized sequence of challenges (e.g., BLINK then SMILE).
   *
   * The UI should display each challenge in order and the native
   * LivenessDetector will track gesture completion via EAR/MAR deltas.
   *
   * @returns LivenessChallengeSequence with ordered challenge list.
   */
  startLivenessChallenge(): LivenessChallengeSequence {
    return requireNativeModule().startLivenessChallenge();
  },

  /**
   * Enroll a user's face embedding in the local cache.
   * The embedding must be a 512-dimensional float32 vector (2048 bytes).
   *
   * @param userId     - Unique user identifier.
   * @param featureData - ArrayBuffer containing 512 float32 values.
   * @returns true if enrollment succeeded.
   */
  enrollUser(userId: string, featureData: ArrayBuffer): boolean {
    if (featureData.byteLength !== 512 * 4) {
      throw new Error(
        `[FaceAuth] Feature data must be 512 floats (2048 bytes), ` +
        `got ${featureData.byteLength} bytes`
      );
    }
    return requireNativeModule().enrollUser(userId, featureData);
  },

  /**
   * Configure the AWS API Gateway endpoint for the sync manager.
   * The sync manager will use this URL to upload attendance records
   * when network connectivity is available.
   *
   * @param url - Full HTTPS URL of the sync endpoint.
   *
   * @example
   * ```ts
   * FaceAuth.setSyncEndpoint('https://api.datalake.in/v1/attendance/sync');
   * ```
   */
  setSyncEndpoint(url: string): void {
    requireNativeModule().setSyncEndpoint(url);
  },

  /**
   * Check if the native module is installed and ready.
   */
  isInstalled(): boolean {
    return getNativeModule() !== null;
  },
};

export default FaceAuth;
