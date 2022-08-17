// Copyright (c) 2020 Cesanta Software Limited
// All rights reserved

#include <signal.h>
#include <string>
#include "mongoose.h"
#include "librador.h"

static int s_debug_level = MG_LL_INFO;
static const char *s_root_dir = ".";
static const char *s_listening_address = "http://0.0.0.0:8000";
static const char *s_enable_hexdump = "no";
static const char *s_ssi_pattern = "#.html";
#define vcc (double)3.3
#define vref ((double)(vcc/2))
#define R4 (double)75000
#define R3 (double)1000000
#define R2 (double)1000
#define R1 (double)1000
int GRAPH_SAMPLES = 1024;

#define formatBool(b) ((b) ? "true" : "false")

// Handle interrupts, like Ctrl-C
static int s_signo;
static void signal_handler(int signo) {
  s_signo = signo;
}

void analogConvert(std::vector<double> *shortPtr, std::vector<double> *doublePtr, int TOP, bool AC, int channel){

    double scope_gain = 1;//(double)(driver->scopeGain);
    double accumulated = 0;
    double accumulated_square = 0;
    double currentVmax = -20;
    double currentVmin = 20;

    double ref = 1.65; //(channel == 1 ? ch1_ref : ch2_ref);
    double frontendGain = 1; //(channel == 1 ? frontendGain_CH1 : frontendGain_CH2);

    double *data = doublePtr->data();
    for (int i=0;i<doublePtr->size();i++){
        data[i] = (shortPtr->at(i) * (vcc/2)) / (frontendGain*scope_gain*TOP);
        //!! if (driver->deviceMode != 7) data[i] += ref;
        #ifdef INVERT_MM
            if(driver->deviceMode == 7) data[i] *= -1;
        #endif

        accumulated += data[i];
        accumulated_square += data[i] * data[i];
        if (data[i] > currentVmax) currentVmax = data[i];
        if (data[i] < currentVmin) currentVmin = data[i];
    }
    /*
    currentVmean  = accumulated / doublePtr->size();
    currentVRMS = sqrt(accumulated_square / doublePtr->size());

    if(AC){
        //Previous measurments are wrong, edit and redo.
        accumulated = 0;
        accumulated_square = 0;
        currentVmax = -20;
        currentVmin = 20;

        for (int i=0;i<doublePtr->size();i++){
            data[i] -= currentVmean;

            accumulated += data[i];
            accumulated_square += (data[i] * data[i]);
            if (data[i] > currentVmax) currentVmax = data[i];
            if (data[i] < currentVmin) currentVmin = data[i];
        }
        currentVmean  = accumulated / doublePtr->size();
        currentVRMS = sqrt(accumulated_square / doublePtr->size());
    }
    */
    //cool_waveform = cool_waveform - AC_offset;
}

// Event handler for the listening connection.
// Simply serve static files from `s_root_dir`
static void cb(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
  if (ev == MG_EV_HTTP_MSG) {
    struct mg_http_message *hm = (mg_http_message*)ev_data, tmp = {0};
    struct mg_str unknown = mg_str_n("?", 1), *cl;
    struct mg_http_serve_opts opts = {0};
    opts.root_dir = s_root_dir;
    opts.ssi_pattern = s_ssi_pattern;

    if (mg_http_match_uri(hm, "/lab/hi")) {
        mg_http_reply(c, 200, "", "hi\n");  // Testing endpoint
    } else if (mg_http_match_uri(hm, "/lab/ver")) {
        auto ver = librador_get_device_firmware_version();
        auto m_ver = librador_get_device_firmware_variant();

        mg_http_reply(c, 200, "", "{version: %u.%u}", ver, m_ver);  // Testing endpoint
    } else if (mg_http_match_uri(hm, "/lab/init")) {
        auto r = librador_init();
        //MG_INFO("Started Librador: %i", r);
        auto i = librador_setup_usb();

        mg_http_reply(c, 200, "", "{init: %d, detected: %d}", r, i);  // Testing endpoint
    } else if (mg_http_match_uri(hm, "/lab/reset")) {
        auto r = librador_reset_device();

        mg_http_reply(c, 200, "", "{reset_flag: %d}", r);  // Testing endpoint
    } else if (mg_http_match_uri(hm, "/lab/mode")) {
        struct mg_str m = mg_http_var(hm->query, mg_str("m")); //m=?
        m = mg_strdup(m); //note: mg non-zero terminate fix
        int modeval = atoi(m.ptr);

        bool success = librador_set_device_mode(modeval) == 0;

        mg_http_reply(c, 200, "", "{success: %s, mode: %d}", formatBool(success), modeval);  // Testing endpoint
    } else if (mg_http_match_uri(hm, "/lab/analog_data")) {
        std::vector<double> data = *librador_get_analog_data(1, 5, 5000, 1, 0);
        std::string dataout;
        for(double d : data)
        {
            dataout.append(std::to_string(d));
            dataout.append("0,");
        }

        dataout.append("0");

        mg_http_reply(c, 200, "", "{data: [%s]}", dataout.c_str());  // Testing endpoint
    } else if (mg_http_match_uri(hm, "/lab/mm/res")) {
        librador_set_device_mode(7); //multimeter mode

        std::vector<double>* data = librador_get_analog_data(1, 5, 5000, 1, 0);
        //data buffer
        std::vector<double> ch1(GRAPH_SAMPLES);
        analogConvert(data, &ch1, 0, false, 1);
        double acc = 0;
        for(double d : ch1)
        {
            acc += d;
        }

        acc = acc / ch1.size();
        double seriesResistance = 1000;
        double multimeterRsource = 0;
        double rtest_para_r = 1/(1/seriesResistance);
        double ch2_ref = 1.65; //??
        double Vm = acc;
        double perturbation = ch2_ref * (rtest_para_r / (R3 + R4 + rtest_para_r));
        Vm = Vm - perturbation;
        double Vin = (multimeterRsource * 2) + 3;
        double Vrat = (Vin-Vm)/Vin;
        double Rp = 1/(1/seriesResistance + 1/(R3+R4));
        double estimated_resistance = ((1-Vrat)/Vrat) * Rp; //Perturbation term on V2 ignored.  V1 = Vin.  V2 = Vin(Rp/(R+Rp)) + Vn(Rtest||R / (R34 + (Rtest||R34));
        //qDebug() << "Vm = " << Vm;
        //estimated_resistance /= 1000; //k


        mg_http_reply(c, 200, "", "{acc: %g, rk: %g, val: %g}", acc, estimated_resistance, Vrat);  // Testing endpoint
    } else {
        //serve static files
        mg_http_serve_dir(c, hm, &opts);
        mg_http_parse((char *) c->send.buf, c->send.len, &tmp);
        cl = mg_http_get_header(&tmp, "Content-Length");
        if (cl == NULL) cl = &unknown;
        MG_INFO(("%.*s %.*s %.*s %.*s", (int) hm->method.len, hm->method.ptr,
                (int) hm->uri.len, hm->uri.ptr, (int) tmp.uri.len, tmp.uri.ptr,
                (int) cl->len, cl->ptr));
    }
  }
  (void) fn_data;
}

static void usage(const char *prog) {
  fprintf(stderr,
          "Mongoose v.%s\n"
          "Usage: %s OPTIONS\n"
          "  -H yes|no - enable traffic hexdump, default: '%s'\n"
          "  -S PAT    - SSI filename pattern, default: '%s'\n"
          "  -d DIR    - directory to serve, default: '%s'\n"
          "  -l ADDR   - listening address, default: '%s'\n"
          "  -v LEVEL  - debug level, from 0 to 4, default: %d\n",
          MG_VERSION, prog, s_enable_hexdump, s_ssi_pattern, s_root_dir,
          s_listening_address, s_debug_level);
  exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
  char path[MG_PATH_MAX] = ".";
  struct mg_mgr mgr;
  struct mg_connection *c;
  int i;

  // Parse command-line flags
  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-d") == 0) {
      s_root_dir = argv[++i];
    } else if (strcmp(argv[i], "-H") == 0) {
      s_enable_hexdump = argv[++i];
    } else if (strcmp(argv[i], "-S") == 0) {
      s_ssi_pattern = argv[++i];
    } else if (strcmp(argv[i], "-l") == 0) {
      s_listening_address = argv[++i];
    } else if (strcmp(argv[i], "-v") == 0) {
      s_debug_level = atoi(argv[++i]);
    } else {
      usage(argv[0]);
    }
  }

  // Root directory must not contain double dots. Make it absolute
  // Do the conversion only if the root dir spec does not contain overrides
  if (strchr(s_root_dir, ',') == NULL) {
    realpath(s_root_dir, path);
    s_root_dir = path;
  }

  // Initialise stuff
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  mg_log_set(s_debug_level);
  mg_mgr_init(&mgr);
  if ((c = mg_http_listen(&mgr, s_listening_address, cb, &mgr)) == NULL) {
    MG_ERROR(("Cannot listen on %s. Use http://ADDR:PORT or :PORT",
              s_listening_address));
    exit(EXIT_FAILURE);
  }
  if (mg_casecmp(s_enable_hexdump, "yes") == 0) c->is_hexdumping = 1;

  // Start infinite event loop
  MG_INFO(("Mongoose version : v%s", MG_VERSION));
  MG_INFO(("Listening on     : %s", s_listening_address));
  MG_INFO(("Web root         : [%s]", s_root_dir));
  while (s_signo == 0) mg_mgr_poll(&mgr, 1000);
  mg_mgr_free(&mgr);
  MG_INFO(("Exiting on signal %d", s_signo));
  //shutdown hardware
  librador_exit();
  return 0;
}