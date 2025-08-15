//--------------------------------------------------------------------------------------
// Copyright (c) 2025 BattleAxeVR. All rights reserved.
//--------------------------------------------------------------------------------------

// Author: Bela Kampis
// Date: April 11, 2025

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

#include "pch.h"
#include "defines.h"

#if ENABLE_PSVR2_EYE_TRACKING_SOCKETS

#include "psvr2_eye_tracking_sockets.h"
#include <glm/glm.hpp>

#pragma comment(lib, "Ws2_32.lib")

namespace BVR 
{

template<typename T> static inline T bvr_clamp(T v, T mn, T mx)
{
	return (v < mn) ? mn : (v > mx) ? mx : v;
}

PSVR2EyeTracker::PSVR2EyeTracker()
{
}

bool PSVR2EyeTracker::connect()
{
	if(!is_connected_)
	{
		WSADATA wsa_data{};
		WSAStartup(MAKEWORD(2, 2), &wsa_data);

		socket_ = socket(AF_INET, SOCK_STREAM, 0);
		{
			unsigned long one = 1;
			ioctlsocket(socket_, FIONBIO, (unsigned long*)&one);
		}

		{
			struct sockaddr_in addr {};
			addr.sin_family = AF_INET;
			addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
			addr.sin_port = htons(IPC_PORT);

			unsigned int retries = SOCKET_CONNECT_RETRIES;
			while(retries) 
			{
				if(!::connect(socket_, (struct sockaddr*)&addr, sizeof(addr)))
				{
					break;
				}
				if(WSAGetLastError() == WSAEISCONN) 
				{
					break;
				}

				Sleep(SOCKET_CONNECT_SLEEP_TIME_MS);
				retries--;
			}

			const bool connected_ok = (retries > 0);

			if(!connected_ok)
			{
				return false;
			}
		}

		HandShakeRequest handshake_request{};
		handshake_request.header.type = REQUEST_HANDSHAKE_;
		handshake_request.header.dataLen = sizeof(handshake_request.payload);
		handshake_request.payload.ipcVersion = IPC_VERSION;
		handshake_request.payload.processId = GetCurrentProcessId();
		::send(socket_, (char*)&handshake_request, sizeof(handshake_request), 0);

		HandShakeResponse handshake_response{};

		unsigned int retries = HANDSHAKE_RETRIES; 
		char* buffer = (char*)&handshake_response;
		int offset = 0;

		while(retries) 
		{
			const auto result = ::recv(socket_, buffer + offset, sizeof(handshake_response) - offset, 0);
			if(result > 0) 
			{
				offset += result;
			}
			if(offset >= sizeof(handshake_response))
			{
				break;
			}

			Sleep(HANDSHAKE_SLEEP_TIME_MS);
			retries--;
		}

		const bool handshake_ok = (retries > 0) 
			&& (handshake_response.header.type == REQUEST_HANDSHAKE_RESULT_) && (handshake_response.payload.result == HANDSHAKE_RESULT_SUCCESS_);

		if(!handshake_ok)
		{
			return false;
		}

		is_connected_ = true;

#if ENABLE_PSVR2_EYE_TRACKING_AUTOMATICALLY
		set_enabled(is_connected_);
#endif
	}

	return is_connected_;
}


void PSVR2EyeTracker::disconnect()
{
	if(is_connected_)
	{
		if(socket_ != INVALID_SOCKET) 
		{
			shutdown(socket_, 2);
			closesocket(socket_);
			socket_ = INVALID_SOCKET;
		}

		WSACleanup();

		is_connected_ = false;
	}
}

bool PSVR2EyeTracker::update_gazes()
{
	if(!is_connected_)
	{
		return false;
	}

	GazeRequest gaze_request{};
	gaze_request.header.type = REQUEST_GAZES_;
	gaze_request.header.dataLen = 0;

	::send(socket_, (char*)&gaze_request, sizeof(gaze_request), 0);

	GazeResponse gaze_response{};

	unsigned int retries = GET_GAZE_RETRIES;
	char* buffer = (char*)&gaze_response;
	int offset = 0;

	while(retries) 
	{
		const auto result = recv(socket_, buffer + offset, sizeof(gaze_response) - offset, 0);

		if(result > 0) 
		{
			offset += result;
		}
		if(offset >= sizeof(gaze_response))
		{
			break;
		}

		Sleep(GET_GAZE_SLEEP_TIME_MS);
		retries--;
	}

	const bool gazes_ok = (retries > 0) && (gaze_response.header.type == REQUEST_GAZES_RESULT_);

	if (gazes_ok)
	{
		const RemoteGazeState& left_gaze = gaze_response.payload.per_eye_gazes_[BVR::LEFT];
		const RemoteGazeState& right_gaze = gaze_response.payload.per_eye_gazes_[BVR::RIGHT];

		//const glm::vec3 left_gaze_dir_vec3 = glm::normalize(glm::vec3(-left_gaze.gaze_dir_.x, left_gaze.gaze_dir_.y, -left_gaze.gaze_dir_.z));
		//const glm::vec3 right_gaze_dir_vec3 = glm::normalize(glm::vec3(-right_gaze.gaze_dir_.x, right_gaze.gaze_dir_.y, -right_gaze.gaze_dir_.z));

		//const XrVector3f left_gaze_dir = { left_gaze_dir_vec3.x, left_gaze_dir_vec3.y, left_gaze_dir_vec3.z };
		//const XrVector3f right_gaze_dir = { right_gaze_dir_vec3.x, right_gaze_dir_vec3.y, right_gaze_dir_vec3.z };

		const XrVector3f left_gaze_dir = { -left_gaze.gaze_dir_.x, left_gaze.gaze_dir_.y, -left_gaze.gaze_dir_.z };
		const XrVector3f right_gaze_dir = { -right_gaze.gaze_dir_.x, right_gaze.gaze_dir_.y, -right_gaze.gaze_dir_.z };

		const bool is_left_valid_and_open = true;// (left_gaze.is_gaze_dir_valid_ && (!left_gaze.is_blink_valid_ || !left_gaze.blink_));
		const bool is_right_valid_and_open = true;// (right_gaze.is_gaze_dir_valid_ && (!right_gaze.is_blink_valid_ || !right_gaze.blink_));

#if ENABLE_PSVR2_EYE_TRACKING_COMBINED_GAZE
		combined_gaze_.is_valid_ = (is_left_valid_and_open || is_right_valid_and_open);

		if (combined_gaze_.is_valid_)
		{/*
			if (is_left_valid_and_open && is_right_valid_and_open)
			{
				const glm::vec3 combined_gaze_dir_vec3 = glm::normalize((left_gaze_dir_vec3 + right_gaze_dir_vec3) * 0.5f);
				combined_gaze_.direction_ = { combined_gaze_dir_vec3.x, combined_gaze_dir_vec3.y, combined_gaze_dir_vec3.z };
			}
			else if (is_left_valid_and_open)
			{
				combined_gaze_.direction_ = left_gaze_dir;
			}
			else if(is_right_valid_and_open)*/
			{
				combined_gaze_.direction_ = right_gaze_dir;
			}
		}
#endif

#if ENABLE_PSVR2_EYE_TRACKING_PER_EYE_GAZES
		per_eye_gazes_[LEFT].direction_ = left_gaze_dir;
		per_eye_gazes_[LEFT].is_valid_ = is_left_valid_and_open;

		per_eye_gazes_[RIGHT].direction_ = right_gaze_dir;
		per_eye_gazes_[RIGHT].is_valid_ = is_right_valid_and_open;
#endif

		return true;
	}

    return false;
}

#if ENABLE_PSVR2_EYE_TRACKING_COMBINED_GAZE
bool PSVR2EyeTracker::is_combined_gaze_available() const
{
	if(!is_connected() || !is_enabled())
	{
		return false;
	}

	return combined_gaze_.is_valid_;
}
#endif


#if ENABLE_PSVR2_EYE_TRACKING_PER_EYE_GAZES
bool PSVR2EyeTracker::is_gaze_available(const int eye) const
{
	if(!is_connected() || !is_enabled())
	{
		return false;
	}

	return per_eye_gazes_[eye].is_valid_;
}
#endif

#if ENABLE_PSVR2_EYE_TRACKING_COMBINED_GAZE
bool PSVR2EyeTracker::get_combined_gaze(XrVector3f& combined_gaze_direction, const bool should_apply_gaze)
{
	(void)should_apply_gaze;

    if (combined_gaze_.is_valid_)
    {
#if ENABLE_GAZE_CALIBRATION
		if(apply_calibration_ && calibrations_[2].is_calibrated())
		{
			combined_gaze_direction = calibrations_[2].apply_calibration(combined_gaze_.direction_);
			return true;
		}
#endif

		combined_gaze_direction = combined_gaze_.direction_;
		return true;
	}

    return false;
}
#endif // ENABLE_PSVR2_EYE_TRACKING_COMBINED_GAZE

#if ENABLE_PSVR2_EYE_TRACKING_PER_EYE_GAZES
bool PSVR2EyeTracker::get_per_eye_gaze(const int eye, XrVector3f& per_eye_gaze_direction, const bool should_apply_gaze)
{
#if AUTO_CALIBRATE
	if(calibrations_[eye].is_calibrating() && (increment_countdown_ > 0))
	{
		increment_countdown_--;

		if(increment_countdown_ == 0)
		{
			increment_raster();
		}
	}
#endif

	if(per_eye_gazes_[eye].is_valid_)
	{
#if ENABLE_GAZE_CALIBRATION
		if (calibrations_[eye].is_calibrating())
		{
			CalibrationPoint& point = calibrations_[eye].get_raster_point();

			if (point.is_calibrated_)
			{
#if AUTO_INCREMENT_ON_CALIBRATION_DONE
				increment_raster();
#endif
			}
			else if (should_apply_gaze)
			{
				const bool sample_ok = point.add_sample(per_eye_gazes_[eye].direction_);

				if (sample_ok)
				{
					if(point.is_calibrated_)
					{
						calibrations_[eye].num_calibrated_++;
					}

					increment_countdown_ = AUTO_INCREMENT_COUNTDOWN;
				}
			}
		}

		if (apply_calibration_ && calibrations_[eye].is_calibrated())
		{
			per_eye_gaze_direction = calibrations_[eye].apply_calibration(per_eye_gazes_[eye].direction_);
			return true;
		}
#else
		(void)should_apply_gaze;
#endif
		
		per_eye_gaze_direction = per_eye_gazes_[eye].direction_;
		return true;
	}

    return false;
}
#endif // ENABLE_PSVR2_EYE_TRACKING_PER_EYE_GAZES

#if ENABLE_GAZE_CALIBRATION
void PSVR2EyeTracker::set_apply_calibration(const bool enabled)
{
	apply_calibration_ = enabled;
}

void PSVR2EyeTracker::toggle_apply_calibration()
{
	apply_calibration_ = !apply_calibration_;
}

void PSVR2EyeTracker::reset_calibrations()
{
#if ENABLE_PSVR2_EYE_TRACKING_PER_EYE_GAZES
	calibrating_eye_index_ = INVALID_INDEX;
	calibrations_[LEFT_CALIBRATION_INDEX].reset_calibration();
	calibrations_[RIGHT_CALIBRATION_INDEX].reset_calibration();
#endif

#if ENABLE_PSVR2_EYE_TRACKING_COMBINED_GAZE
	calibrations_[COMBINED_CALIBRATION_INDEX].reset_calibration();
#endif
}

bool PSVR2EyeTracker::load_calibrations()
{
	bool success = true;

#if ENABLE_PSVR2_EYE_TRACKING_PER_EYE_GAZES
	success &= calibrations_[LEFT_CALIBRATION_INDEX].load_calibration();
	success &= calibrations_[RIGHT_CALIBRATION_INDEX].load_calibration();
#endif

#if ENABLE_PSVR2_EYE_TRACKING_COMBINED_GAZE
	success &= calibrations_[COMBINED_CALIBRATION_INDEX].load_calibration();
#endif

	return success;
}

bool PSVR2EyeTracker::save_calibrations()
{
	bool success = true;

#if ENABLE_PSVR2_EYE_TRACKING_PER_EYE_GAZES
	success &= calibrations_[LEFT_CALIBRATION_INDEX].save_calibration();
	success &= calibrations_[RIGHT_CALIBRATION_INDEX].save_calibration();
#endif

#if ENABLE_PSVR2_EYE_TRACKING_COMBINED_GAZE
	success &= calibrations_[COMBINED_CALIBRATION_INDEX].save_calibration();
#endif

	return success;
}

bool PSVR2EyeTracker::is_calibrating() const
{
#if ENABLE_PSVR2_EYE_TRACKING_PER_EYE_GAZES
	if (is_eye_calibrating(LEFT) || is_eye_calibrating(RIGHT))
	{
		return true;
	}
#endif

#if ENABLE_PSVR2_EYE_TRACKING_COMBINED_GAZE
	if(is_combined_calibrating())
	{
		return true;
	}
#endif

	return false;
}

bool PSVR2EyeTracker::is_fully_calibrated() const
{
	bool calibrated = true;

#if ENABLE_PSVR2_EYE_TRACKING_PER_EYE_GAZES
	calibrated &= is_eye_calibrated(LEFT);
	calibrated &= is_eye_calibrated(RIGHT);
#endif

#if ENABLE_PSVR2_EYE_TRACKING_COMBINED_GAZE
	calibrated &= is_combined_calibrated();
#endif

	return calibrated;
}

void PSVR2EyeTracker::start_calibrating()
{
	if (is_fully_calibrated() || is_calibrating())
	{
		return;
	}

#if ENABLE_PSVR2_EYE_TRACKING_PER_EYE_GAZES
	if (!is_eye_calibrated(LEFT))
	{
		start_eye_calibration(LEFT);
		return;
	}
	else if(is_eye_calibrated(LEFT) && !is_eye_calibrated(RIGHT))
	{
		start_eye_calibration(RIGHT);
		return;
	}
#endif

#if ENABLE_PSVR2_EYE_TRACKING_COMBINED_GAZE
	if(!is_combined_calibrated())
	{
		start_combined_calibration();
		return;
	}
#endif
}

void PSVR2EyeTracker::stop_calibrating()
{

}

void PSVR2EyeTracker::increment_raster()
{
#if ENABLE_PSVR2_EYE_TRACKING_PER_EYE_GAZES
	if(calibrating_eye_index_ != INVALID_INDEX)
	{
		if(calibrations_[calibrating_eye_index_].is_calibrating())
		{
			return calibrations_[calibrating_eye_index_].increment_raster();
		}
	}
#endif

#if ENABLE_PSVR2_EYE_TRACKING_COMBINED_GAZE
	if(calibrations_[COMBINED_CALIBRATION_INDEX].is_calibrating())
	{
		return calibrations_[COMBINED_CALIBRATION_INDEX].increment_raster();
	}
#endif
}

GLMPose PSVR2EyeTracker::get_calibration_cube() const
{
#if ENABLE_PSVR2_EYE_TRACKING_PER_EYE_GAZES
	if(calibrating_eye_index_ != INVALID_INDEX)
	{
		if(calibrations_[calibrating_eye_index_].is_calibrating())
		{
			return calibrations_[calibrating_eye_index_].get_calibration_cube();
		}
	}
#endif

#if ENABLE_PSVR2_EYE_TRACKING_COMBINED_GAZE
	if(calibrations_[COMBINED_CALIBRATION_INDEX].is_calibrating())
	{
		return calibrations_[COMBINED_CALIBRATION_INDEX].get_calibration_cube();
	}
#endif

	return {};
}

#endif

} // BVH


#endif // ENABLE_PSVR2_EYE_TRACKING_SOCKETS
