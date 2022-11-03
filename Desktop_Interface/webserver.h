#include <signal.h>
#include <string>
#include "mongoose.h"
#include "mainwindow.h"
#include <QThread>

static int s_debug_level = MG_LL_INFO;
static int MODE_MM_RES = 1;
static int MODE_DEFAULT = 0;
static const char *s_root_dir = ".";
static const char *s_listening_address = "http://0.0.0.0:8000";
static const char *s_enable_hexdump = "no";
static const char *s_ssi_pattern = "#.html";

MainWindow* _w = NULL;

static void fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data)
{
    //
  if (ev == MG_EV_HTTP_MSG) {
    struct mg_http_message *hm = (mg_http_message*)ev_data, tmp = {0};
    struct mg_str unknown = mg_str_n("?", 1), *cl;
    struct mg_http_serve_opts opts = {0};
    opts.root_dir = s_root_dir;
    opts.ssi_pattern = s_ssi_pattern;

    qDebug() << "http req";

    if (_w == NULL)
    {
        mg_http_reply(c, 500, "", "not ready\n");  // Testing endpoint
        return;
    }

    if (mg_http_match_uri(hm, "/lab/hi"))
    {
        mg_http_reply(c, 200, "", "hi\n");  // Testing endpoint
    } else if (mg_http_match_uri(hm, "/lab/baud"))
    {    
        mg_http_reply(c, 200, "", "{baud: %d}", _w->ui->controller_iso->baudRate_CH1);
    } else if (mg_http_match_uri(hm, "/lab/ch1"))
    {    
        mg_http_reply(c, 200, "", "{ref: %g}", _w->ui->controller_iso->ch1_ref); 
        //
    } else if (mg_http_match_uri(hm, "/ch1/mean"))
    {    
        mg_http_reply(c, 200, "", "{mean: %g}", _w->ui->voltageInfoMeanDisplay_CH1->value());
    } else if (mg_http_match_uri(hm, "/mm/volt"))
    {
        _w->ui->scopeGroup_CH1->setChecked(false);
        _w->ui->scopeGroup_CH2->setChecked(false);
        _w->ui->multimeterGroup->setChecked(true);

        _w->ui->multimeterModeSelect->setCurrentIndex(2);
        _w->ui->multimeterResistanceSelect->setValue(1000);

        mg_http_reply(c, 200, "", "{vmean: %g}", _w->ui->multimeterResistanceLabel->text());
    } else {

        //mg_http_reply(c, 200, "", "no response\n");  // Testing endpoint
    }
  }
}

class MyThread : public QThread
{
public:
    // constructor
    // set name using initializer
    explicit MyThread() {};

    void setup()
    {

        mg_log_set(s_debug_level);
        mg_mgr_init(&mgr);

        if (mg_http_listen(&mgr, "0.0.0.0:8000", fn, NULL) != NULL)     // Create listening connection
        {
            this->start();
        }
        else
        {
            qDebug() << "WEBSERVER FAILURE!!!";
        }
    }

    // overriding the QThread's run() method
    void run()
    {
        qDebug() << " >WEBSERVER THREAD started.";

        while (s_signo == 0) mg_mgr_poll(&mgr, 1000);
    }
private:
    int s_signo = 0;
    struct mg_mgr mgr;
    struct mg_connection *c;
};

MyThread th;

void init_webserver()
{
    th.setup();
}

void link_webserver(MainWindow* w)
{
    _w = w;
}
