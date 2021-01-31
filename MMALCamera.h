#include "interface/mmal/mmal.h"
#include "interface/mmal/mmal_logging.h"
#include "interface/mmal/mmal_buffer.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_connection.h"

class MMALCamera
{
    public:
        MMALCamera();
        bool startPreview();
        bool endPreview();
        ~MMALCamera();

    private:
        // Globals private variables for starting and stopping camera preview
        MMAL_COMPONENT_T *previewComponent;
        MMAL_COMPONENT_T *cameraComponent;
        MMAL_CONNECTION_T *previewConnection;
        MMAL_PORT_T *previewPort;
        MMAL_PORT_T *cameraPreviewPort;
        MMAL_PORT_T *previewInputPort;
        
        // State variables
        bool previewState;

        // Callback for MMAL
        static void default_camera_control_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
        
        // Create / destroy functions
        MMAL_STATUS_T create_preview(void);
        MMAL_STATUS_T create_camera(void);
        void destroy_preview(void);
        void destroy_camera(void);

        // Port connection / disconnection
        MMAL_STATUS_T connect_ports(MMAL_PORT_T *output_port, MMAL_PORT_T *input_port);
        void disconnect_ports();
};
