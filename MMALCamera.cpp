/*
Copyright (c) 2020, Aritro Saha.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <cstdio>
#include "MMALCamera.h"

#pragma region Private Functions

// Callback func for camera data loop, dumps stats
void MMALCamera::default_camera_control_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
   fprintf(stderr, "Camera control callback  cmd=0x%08x", buffer->cmd);

   if (buffer->cmd == MMAL_EVENT_PARAMETER_CHANGED)
   {
      MMAL_EVENT_PARAMETER_CHANGED_T *param = (MMAL_EVENT_PARAMETER_CHANGED_T *)buffer->data;
      switch (param->hdr.id)
      {
      case MMAL_PARAMETER_CAMERA_SETTINGS:
      {
         MMAL_PARAMETER_CAMERA_SETTINGS_T *settings = (MMAL_PARAMETER_CAMERA_SETTINGS_T *)param;
         vcos_log_error("Exposure now %u, analog gain %u/%u, digital gain %u/%u",
                        settings->exposure,
                        settings->analog_gain.num, settings->analog_gain.den,
                        settings->digital_gain.num, settings->digital_gain.den);
         vcos_log_error("AWB R=%u/%u, B=%u/%u",
                        settings->awb_red_gain.num, settings->awb_red_gain.den,
                        settings->awb_blue_gain.num, settings->awb_blue_gain.den);
      }
      break;
      }
   }
   else if (buffer->cmd == MMAL_EVENT_ERROR)
   {
      vcos_log_error("No data received from the sensor");
   }
   else
   {
      vcos_log_error("Received an unexpected camera control callback event: 0x%08x", buffer->cmd);
   }

   mmal_buffer_header_release(buffer);
}

// Create the preview component, assign it to preview_component
// Returns MMAL_STATUS
MMAL_STATUS_T MMALCamera::create_preview()
{
   MMAL_COMPONENT_T *previewTmp = NULL;
   MMAL_STATUS_T status = mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_RENDERER, &previewTmp);

   // Check if component creation went through
   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Unable to create preview component");
      goto error;
   }

   // No input ports found, raise error and clean up
   if (!previewTmp->input_num)
   {
      status = MMAL_ENOSYS;
      vcos_log_error("No input ports found on component");
      goto error;
   }

   // Get preview port from component (0 is used because this is the preview port)
   previewPort = previewTmp->input[0];

   // Setup how the preview will be displayed
   MMAL_DISPLAYREGION_T param;
   param.hdr.id = MMAL_PARAMETER_DISPLAYREGION;
   param.hdr.size = sizeof(MMAL_DISPLAYREGION_T);

   // Set display layer
   param.set = MMAL_DISPLAY_SET_LAYER;
   param.layer = 2;

   // Set fullscreen
   param.set |= MMAL_DISPLAY_SET_FULLSCREEN;
   param.fullscreen = 1;

   // Set transparency
   param.set |= MMAL_DISPLAY_SET_ALPHA;
   param.alpha = 255;

   // Add these parameters to the preview port
   status = mmal_port_parameter_set(previewPort, &param.hdr);

   // Can't set port params, raise error and clean up
   if (status != MMAL_SUCCESS && status != MMAL_ENOSYS)
   {
      vcos_log_error("unable to set preview port parameters (%u)", status);
      goto error;
   }

   // Enable the component
   status = mmal_component_enable(previewTmp);
   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Unable to enable preview component (%u)", status);
      goto error;
   }

   // Since everything went well, we can change the global pointer to the local variable we manipulated
   previewComponent = previewTmp;

   return status;

// Error cleanup code (TODO: Change to a function?)
error:
   if (previewTmp)
      mmal_component_destroy(previewTmp);
   return status;
}

// Destroys the preview component
void MMALCamera::destroy_preview()
{
   if (previewComponent)
   {
      mmal_component_destroy(previewComponent);
      previewComponent = NULL;
   }
}

// Creates the camera module
MMAL_STATUS_T MMALCamera::create_camera()
{
   // TODO: Format this code properly so that a still port is not set up (no need for it)
   MMAL_COMPONENT_T *camera = 0;
   MMAL_ES_FORMAT_T *format;
   MMAL_PORT_T *preview_port = NULL;
   MMAL_STATUS_T status;

   // Create component
   status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA, &camera);

   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Failed to create camera component");
      if (camera)
         mmal_component_destroy(camera);
      return status;
   }

   MMAL_PARAMETER_INT32_T camera_num =
       {{MMAL_PARAMETER_CAMERA_NUM, sizeof(camera_num)}, 0};

   status = mmal_port_parameter_set(camera->control, &camera_num.hdr);

   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Could not select camera : error %d", status);
      if (camera)
         mmal_component_destroy(camera);
      return status;
   }

   if (!camera->output_num)
   {
      status = MMAL_ENOSYS;
      vcos_log_error("Camera doesn't have output ports");
      if (camera)
         mmal_component_destroy(camera);
      return status;
   }

   status = mmal_port_parameter_set_uint32(camera->control, MMAL_PARAMETER_CAMERA_CUSTOM_SENSOR_CONFIG, 0);

   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Could not set sensor mode : error %d", status);
      if (camera)
         mmal_component_destroy(camera);
      return status;
   }

   preview_port = camera->output[0];

   // Enable the camera, and tell it its control callback function
   status = mmal_port_enable(camera->control, default_camera_control_callback);

   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Unable to enable control port : error %d", status);
      if (camera)
         mmal_component_destroy(camera);
      return status;
   }

   // Setup cam settings
   MMAL_PARAMETER_CAMERA_CONFIG_T cam_config = {
       {MMAL_PARAMETER_CAMERA_CONFIG, sizeof(cam_config)},
       .max_stills_w = 1280,
       .max_stills_h = 720,
       .stills_yuv422 = 0,
       .one_shot_stills = 1,
       .max_preview_video_w = 1280,
       .max_preview_video_h = 720,
       .num_preview_video_frames = 3,
       .stills_capture_circular_buffer_height = 0,
       .fast_preview_resume = 0,
       .use_stc_timestamp = MMAL_PARAM_TIMESTAMP_MODE_RESET_STC};

   cam_config.max_preview_video_w = 1280;
   cam_config.max_preview_video_h = 720;

   // Apply settings
   mmal_port_parameter_set(camera->control, &cam_config.hdr);

   // Change preview port format
   format = preview_port->format;
   format->encoding = MMAL_ENCODING_OPAQUE;
   format->encoding_variant = MMAL_ENCODING_I420;

   // Change preview port settings
   format->es->video.width = VCOS_ALIGN_UP(1280, 32);
   format->es->video.height = VCOS_ALIGN_UP(720, 16);
   format->es->video.crop.x = 0;
   format->es->video.crop.y = 0;
   format->es->video.crop.width = 1280;
   format->es->video.crop.height = 720;
   format->es->video.frame_rate.num = 0;
   format->es->video.frame_rate.den = 1;

   // Apply new settings and do any required error handling
   status = mmal_port_format_commit(preview_port);
   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Preview could not be set");
      if (camera)
         mmal_component_destroy(camera);
      return status;
   }

   // Enable the component
   status = mmal_component_enable(camera);

   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("camera component couldn't be enabled");
      if (camera)
         mmal_component_destroy(camera);
      return status;
   }

   // Change reference
   cameraComponent = camera;

   return status;
}

// Destroy camera
void MMALCamera::destroy_camera()
{
   if (cameraComponent)
   {
      mmal_component_destroy(cameraComponent);
      cameraComponent = NULL;
   }
}

// Connects two ports togethers, this is used to link the preview port and the camera port
MMAL_STATUS_T MMALCamera::connect_ports(MMAL_PORT_T *output_port, MMAL_PORT_T *input_port)
{
   MMAL_STATUS_T status;

   // Create connection
   status = mmal_connection_create(&previewConnection, output_port, input_port, MMAL_CONNECTION_FLAG_TUNNELLING | MMAL_CONNECTION_FLAG_ALLOCATION_ON_INPUT);

   // Enable if everything went well, destroy if not
   if (status == MMAL_SUCCESS)
   {
      status = mmal_connection_enable(previewConnection);
   }
   else
   {
      mmal_connection_destroy(previewConnection);
   }

   return status;
}

void MMALCamera::disconnect_ports()
{
   // Only destroy if it exists
   if (previewConnection)
   {
      mmal_connection_destroy(previewConnection);
      previewConnection = NULL;
   }
}

#pragma endregion

#pragma region Public Functions

MMALCamera::MMALCamera()
{
   // Initialize variables
   previewConnection = NULL;

   // Create components
   create_camera();
   create_preview();

   // Get the ports
   cameraPreviewPort = cameraComponent->output[0];
   previewInputPort = previewComponent->input[0];
}

// Starts fullscreen camera preview
bool MMALCamera::startPreview()
{
   if (previewState)
   {
      // Already showing something to string, don't do anything
      return false;
   }

   MMAL_STATUS_T state = connect_ports(cameraPreviewPort, previewInputPort);
   if (state == MMAL_SUCCESS)
   {
      // Successfully enabled, toggle previewState var and return true
      previewState = true;
      return true;
   }

   return false;
}

// Closes fullscreen camera preview
bool MMALCamera::endPreview()
{
   if (!previewState)
   {
      // Already not showing, don't do anything
      return false;
   }

   // Disconnect ports, toggle state, and return false
   disconnect_ports();
   previewState = false;

   return false;
}

MMALCamera::~MMALCamera()
{
   // Destroy the connection first if exists
   disconnect_ports();

   // Destroy preview and camera
   destroy_preview();
   destroy_camera();
}
#pragma endregion
