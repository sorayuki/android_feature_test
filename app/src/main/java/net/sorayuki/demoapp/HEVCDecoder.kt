package net.sorayuki.demoapp

import android.media.MediaCodec
import android.media.MediaCodecInfo
import android.media.MediaFormat
import java.io.IOException

class YUVFrame {
    var width: Int = 0
    var height: Int = 0
    var y: ByteArray? = null
    var yStride: Int = 0
    var u: ByteArray? = null
    var uStride: Int = 0
    var v: ByteArray? = null
    var vStride: Int = 0
}

class HEVCDecoder {
    companion object {
        private const val MIME_TYPE = MediaFormat.MIMETYPE_VIDEO_HEVC
        private const val DECODE_TIMEOUT_US = 10000L // 10ms

        /**
         * Decodes a single Annex B HEVC frame bitstream into a YUVFrame.
         * The video dimensions are determined from the bitstream itself.
         *
         * @param bitstream The raw HEVC bitstream for a single frame, compliant with Annex B.
         * @return A YUVFrame containing the decoded image data, or null on failure.
         */
        fun decode(bitstream: ByteArray): YUVFrame? {
            var decoder: MediaCodec? = null
            try {
                decoder = MediaCodec.createDecoderByType(MIME_TYPE)
                // We must provide a format, even with placeholder dimensions.
                // The decoder will determine the actual size from the bitstream (SPS/PPS).
                val format = MediaFormat.createVideoFormat(MIME_TYPE, 640, 480)
                format.setInteger(MediaFormat.KEY_COLOR_FORMAT, MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420Flexible)
                decoder.configure(format, null, null, 0)
                decoder.start()

                var inputDone = false
                var outputDone = false

                // --- Input Stage ---
                val inputBufferIndex = decoder.dequeueInputBuffer(DECODE_TIMEOUT_US)
                if (inputBufferIndex >= 0) {
                    val inputBuffer = decoder.getInputBuffer(inputBufferIndex)
                    inputBuffer?.put(bitstream)
                    // Queue the single frame and signal the end of the stream
                    decoder.queueInputBuffer(
                        inputBufferIndex,
                        0,
                        bitstream.size,
                        0,
                        MediaCodec.BUFFER_FLAG_END_OF_STREAM
                    )
                    inputDone = true
                } else {
                    // If we can't get an input buffer, we can't proceed.
                    return null
                }

                // --- Output Stage ---
                val bufferInfo = MediaCodec.BufferInfo()
                while (!outputDone) {
                    val outputBufferIndex = decoder.dequeueOutputBuffer(bufferInfo, DECODE_TIMEOUT_US)

                    when (outputBufferIndex) {
                        MediaCodec.INFO_OUTPUT_FORMAT_CHANGED -> {
                            // The format is now known. The actual decoding will happen next.
                            // You could inspect decoder.outputFormat here if needed.
                        }
                        MediaCodec.INFO_TRY_AGAIN_LATER -> {
                            // No output available yet
                        }
                        else -> {
                            if (outputBufferIndex >= 0) {
                                // We have a decoded frame
                                val image = decoder.getOutputImage(outputBufferIndex)
                                if (image != null) {
                                    val yuvFrame = YUVFrame()
                                    yuvFrame.width = image.width
                                    yuvFrame.height = image.height

                                    // Y plane
                                    val yPlane = image.planes[0]
                                    val yBuffer = yPlane.buffer
                                    yuvFrame.yStride = yPlane.rowStride
                                    yuvFrame.y = ByteArray(yBuffer.remaining())
                                    yBuffer.get(yuvFrame.y)

                                    // U plane
                                    val uPlane = image.planes[1]
                                    val uBuffer = uPlane.buffer
                                    yuvFrame.uStride = uPlane.rowStride
                                    yuvFrame.u = ByteArray(uBuffer.remaining())
                                    uBuffer.get(yuvFrame.u)

                                    // V plane
                                    val vPlane = image.planes[2]
                                    val vBuffer = vPlane.buffer
                                    yuvFrame.vStride = vPlane.rowStride
                                    yuvFrame.v = ByteArray(vBuffer.remaining())
                                    vBuffer.get(yuvFrame.v)

                                    image.close()
                                    // Since we only need the first frame, we can return immediately.
                                    return yuvFrame
                                }

                                decoder.releaseOutputBuffer(outputBufferIndex, false)

                                if ((bufferInfo.flags and MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) {
                                    outputDone = true
                                }
                            }
                        }
                    }
                }

            } catch (e: Exception) {
                // Covers IOException, IllegalStateException, etc.
                e.printStackTrace()
            } finally {
                try {
                    decoder?.stop()
                    decoder?.release()
                } catch (e: IllegalStateException) {
                    e.printStackTrace()
                }
            }
            return null // Return null if decoding fails or no frame is produced
        }
    }
}
