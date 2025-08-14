//--------------------------------------------------------------------------------------
// Copyright (c) 2025 BattleAxeVR. All rights reserved.
//--------------------------------------------------------------------------------------

// Author: Bela Kampis
// Date: July 5th, 2025

// MIT License
//
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef PSVR2_EYE_TRACKING_H
#define PSVR2_EYE_TRACKING_H

#include "defines.h"

#if ENABLE_PSVR2_EYE_TRACKING

#include <stdint.h>

#define PSVR2_SERVER_NAMED_PIPE_NAME "\\\\.\\pipe\\PlaystationVR2ServerPipe"
#define PSVR2_SERVER_CONNECT_WAIT_TIME 5000

#if ENABLE_GAZE_CALIBRATION
#include "gaze_calibration.h"

#define LEFT_CALIBRATION_INDEX LEFT
#define RIGHT_CALIBRATION_INDEX RIGHT
#define COMBINED_CALIBRATION_INDEX RIGHT + 1
#define NUM_CALIBRATIONS (COMBINED_CALIBRATION_INDEX+1) // Indices LEFT = 0 / RIGHT = 1 / COMBINED = 2
#endif

namespace BVR 
{
	// No OpenXR dependency in this repo -- yet
	struct XrVector3f
	{
		float    x;
		float    y;
		float    z;
	};

	struct XRGazeState
	{
		XrVector3f direction_ = { 0.0f, 0.0f, -1.0f };
		bool is_valid_ = false;
	};

	struct AllXRGazeStates
	{
		XRGazeState combined_gaze_;
		XRGazeState per_eye_gazes_[BVR::NUM_EYES];
	};

	enum RequestType
	{
		UNKNOWN_,
		START_HANDSHAKE_,
		GET_GAZES_,
	};

	enum ResponseType
	{
		ERROR_,
		HANDSHAKE_OK_,
		GET_GAZES_OK_,
	};

	struct Request
	{
		RequestType type_;
		Request() : type_(RequestType::UNKNOWN_) {}
		Request(RequestType type) : type_(type) {}
	};

	struct Response
	{
		ResponseType type_;
		AllXRGazeStates gazes_;

		Response() : type_(ResponseType::ERROR_), gazes_{} {}
		Response(ResponseType type) : type_(type), gazes_{} {}
	};

    class PSVR2EyeTracker
    {
    public:
        PSVR2EyeTracker();

        bool connect();
        void disconnect();
        bool update_gazes();

		const bool is_connected() const { return is_connected_; }
		const bool is_enabled() const { return is_enabled_; }

		void set_enabled(const bool enabled)
		{
			is_enabled_ = is_connected_ && enabled;
		}

		float get_ipd_meters() const
		{
			return ipd_meters_;
		}

		void set_ipd_meters(const float ipd_meters)
		{
			ipd_meters_ = ipd_meters;
		}

#if ENABLE_PSVR2_EYE_TRACKING_COMBINED_GAZE
        bool is_combined_gaze_available() const;
        bool get_combined_gaze(XrVector3f& combined_gaze_direction, const bool should_apply_gaze);

#if ENABLE_GAZE_CALIBRATION
		bool is_combined_calibrated() const 
		{
			return calibrations_[COMBINED_CALIBRATION_INDEX].is_calibrated();
		}

		bool is_combined_calibrating() const
		{
			return calibrations_[COMBINED_CALIBRATION_INDEX].is_calibrating();
		}

		void start_combined_calibration()
		{
			calibrations_[COMBINED_CALIBRATION_INDEX].start_calibration();
		}

		void stop_combined_calibration()
		{
			calibrations_[COMBINED_CALIBRATION_INDEX].stop_calibration();
		}
#endif

#endif // ENABLE_PSVR2_EYE_TRACKING_COMBINED_GAZE
        
#if ENABLE_PSVR2_EYE_TRACKING_PER_EYE_GAZES
        bool is_gaze_available(const int eye) const;
        bool get_per_eye_gaze(const int eye, XrVector3f& per_eye_gaze_direction, const bool should_apply_gaze);

#if ENABLE_GAZE_CALIBRATION
		bool is_eye_calibrated(const int eye) const
		{
			return calibrations_[eye].is_calibrated();
		}

		bool is_eye_calibrating(const int eye) const
		{
			if(is_eye_calibrated(eye))
			{
				return false;
			}

			return (calibrating_eye_index_ == eye);
		}

		int get_calibrating_eye_index() const
		{
			return calibrating_eye_index_;
		}

		void start_eye_calibration(const int eye)
		{
			if((calibrating_eye_index_ != INVALID_INDEX) && (calibrating_eye_index_ != eye))
			{
				stop_eye_calibration();
			}

			if(!is_eye_calibrated(eye))
			{
				calibrations_[eye].start_calibration();
				calibrating_eye_index_ = eye;
			}
			else
			{
				calibrating_eye_index_ = INVALID_INDEX;
			}
		}

		void stop_eye_calibration()
		{
			if (calibrating_eye_index_ != INVALID_INDEX)
			{
				calibrations_[calibrating_eye_index_].stop_calibration();
			}
			calibrating_eye_index_ = INVALID_INDEX;
		}
#endif

#endif // ENABLE_PSVR2_EYE_TRACKING_PER_EYE_GAZES

#if ENABLE_GAZE_CALIBRATION
		void set_apply_calibration(const bool enabled);
		void toggle_apply_calibration();
		void reset_calibrations();
		bool load_calibrations();
		bool save_calibrations();

		bool is_calibrating() const;
		bool is_fully_calibrated() const;

		void start_calibrating();
		void stop_calibrating();

		void increment_raster();
		GLMPose get_calibration_cube() const;
#endif

		int increment_countdown_ = 0;

    private:
        bool is_connected_ = false;
        bool is_enabled_ = false;

		float ipd_meters_ = 0.0f;// 0.067f;

#if ENABLE_PSVR2_EYE_TRACKING_COMBINED_GAZE
		XRGazeState combined_gaze_;
#endif

#if ENABLE_PSVR2_EYE_TRACKING_PER_EYE_GAZES
		XRGazeState per_eye_gazes_[NUM_EYES];
#endif

#if ENABLE_GAZE_CALIBRATION
		GazeCalibration calibrations_[NUM_CALIBRATIONS];
		int calibrating_eye_index_ = INVALID_INDEX;
		bool apply_calibration_ = false;
#endif

		bool send_and_receive(const Request& request, Response& response) const;
		bool send_request(const Request& request) const;
		bool receive_request(Response& response) const;

		HANDLE named_pipe_handle_ = INVALID_HANDLE_VALUE;

    };
}


#endif // ENABLE_PSVR2_EYE_TRACKING


#endif // PSVR2_EYE_TRACKING_H


