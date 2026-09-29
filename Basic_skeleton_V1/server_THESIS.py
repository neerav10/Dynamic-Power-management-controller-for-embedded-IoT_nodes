from flask import Flask, request, jsonify, render_template_string
from datetime import datetime
from collections import deque

app = Flask(__name__)


# =========================================================
# TELEMETRY BUFFER
# =========================================================

MAX_DATA_POINTS = 30

telemetry_history = deque(
    maxlen=MAX_DATA_POINTS
)


# =========================================================
# DASHBOARD
# =========================================================

DASHBOARD_HTML = """
<!DOCTYPE html>

<html lang="en">

<head>

<meta charset="UTF-8">

<meta name="viewport"
      content="width=device-width, initial-scale=1.0">

<title>Wildfire Detection Telemetry</title>

<script src="https://cdn.jsdelivr.net/npm/chart.js">
</script>


<style>

body {
    font-family:
        -apple-system,
        BlinkMacSystemFont,
        "Segoe UI",
        Roboto,
        sans-serif;

    background-color: #0f172a;

    color: #f8fafc;

    margin: 0;

    padding: 20px;
}


.container {
    max-width: 900px;

    margin: auto;
}


.header {

    display: flex;

    justify-content: space-between;

    align-items: center;

    border-bottom:
        1px solid #334155;

    padding-bottom: 12px;

    margin-bottom: 20px;
}


.status-badge {

    background-color: #10b981;

    color: white;

    padding: 4px 12px;

    border-radius: 9999px;

    font-size: 0.85rem;

    font-weight: 600;
}


.cards {

    display: grid;

    grid-template-columns:
        1fr 1fr;

    gap: 16px;

    margin-bottom: 20px;
}


.card {

    background-color: #1e293b;

    padding: 16px;

    border-radius: 8px;

    border: 1px solid #334155;
}


.card-title {

    font-size: 0.875rem;

    color: #94a3b8;

    margin-bottom: 4px;
}


.card-value {

    font-size: 1.75rem;

    font-weight: 700;
}


.chart-container {

    background-color: #1e293b;

    padding: 20px;

    border-radius: 8px;

    border: 1px solid #334155;
}

</style>

</head>


<body>

<div class="container">


<div class="header">

<h2>
Wildfire Detection Node
</h2>

<span
    class="status-badge"
    id="node-status">

Waiting for data...

</span>

</div>


<div class="cards">


<div class="card">

<div class="card-title">
Temperature
</div>

<div
    class="card-value"
    id="latest-temp">

-- °C

</div>

</div>


<div class="card">

<div class="card-title">
MQ-2 Gas / Smoke
</div>

<div
    class="card-value"
    id="latest-mq2">

-- PPM

</div>

</div>


</div>


<div class="chart-container">

<canvas id="telemetryChart">
</canvas>

</div>


</div>


<script>


const ctx =
    document
    .getElementById(
        'telemetryChart'
    )
    .getContext('2d');


const chart = new Chart(
    ctx,
    {

        type: 'line',

        data: {

            labels: [],

            datasets: [

                {
                    label:
                        'Temperature (°C)',

                    data: [],

                    yAxisID: 'y',

                    tension: 0.3,

                    fill: false
                },


                {
                    label:
                        'MQ-2 (PPM)',

                    data: [],

                    yAxisID: 'y1',

                    tension: 0.3,

                    fill: false
                }

            ]
        },


        options: {

            responsive: true,


            scales: {

                x: {
                    ticks: {
                        color: '#94a3b8'
                    },

                    grid: {
                        color: '#334155'
                    }
                },


                y: {

                    type: 'linear',

                    display: true,

                    position: 'left',

                    title: {

                        display: true,

                        text:
                            'Temperature (°C)'
                    }
                },


                y1: {

                    type: 'linear',

                    display: true,

                    position: 'right',

                    title: {

                        display: true,

                        text:
                            'MQ-2 (PPM)'
                    },

                    grid: {

                        drawOnChartArea:
                            false
                    }
                }
            },


            plugins: {

                legend: {

                    labels: {

                        color:
                            '#f8fafc'
                    }
                }
            }
        }
    }
);


// =========================================================
// FETCH TELEMETRY
// =========================================================

async function fetchTelemetry()
{

    try
    {

        const response =
            await fetch(
                '/api/history'
            );


        const data =
            await response.json();


        if (data.length > 0)
        {

            const latest =
                data[data.length - 1];


            document
                .getElementById(
                    'latest-temp'
                )
                .textContent =
                `${latest.temperature.toFixed(1)} °C`;


            document
                .getElementById(
                    'latest-mq2'
                )
                .textContent =
                `${latest.mq2_ppm.toFixed(1)} PPM`;


            document
                .getElementById(
                    'node-status'
                )
                .textContent =
                `Node: ${latest.node_id}`;


            chart.data.labels =
                data.map(
                    entry =>
                        entry.timestamp
                );


            chart.data.datasets[0].data =
                data.map(
                    entry =>
                        entry.temperature
                );


            chart.data.datasets[1].data =
                data.map(
                    entry =>
                        entry.mq2_ppm
                );


            chart.update();

        }

    }

    catch (err)
    {

        console.error(
            "Polling error:",
            err
        );

    }

}


// Poll server every 2 seconds

setInterval(
    fetchTelemetry,
    2000
);


fetchTelemetry();

</script>


</body>

</html>
"""


# =========================================================
# DASHBOARD ROUTE
# =========================================================

@app.route('/', methods=['GET'])
def index():

    return render_template_string(
        DASHBOARD_HTML
    )


# =========================================================
# HISTORY API
# =========================================================

@app.route(
    '/api/history',
    methods=['GET']
)
def get_history():

    return jsonify(
        list(telemetry_history)
    ), 200


# =========================================================
# RECEIVE DATA FROM ESP32
# =========================================================

@app.route(
    '/api/data',
    methods=['GET', 'POST'],
    strict_slashes=False
)
def receive_data():

    if request.method == 'GET':

        return jsonify({
            "status": "active",

            "message":
                "Send POST requests containing JSON telemetry."
        }), 200


    if not request.is_json:

        return jsonify({

            "status": "error",

            "message":
                "Content-Type must be application/json"

        }), 400


    try:

        payload = request.get_json()


        data_entry = {

            "node_id":
                payload.get(
                    "node_id",
                    "unknown_node"
                ),

            "temperature":
                float(
                    payload.get(
                        "temperature",
                        0.0
                    )
                ),

            "mq2_ppm":
                float(
                    payload.get(
                        "mq2_ppm",
                        0.0
                    )
                ),

            "timestamp":
                datetime.now().strftime(
                    "%H:%M:%S"
                )
        }


        telemetry_history.append(
            data_entry
        )


        print(
            f"Logged Telemetry: "
            f"{data_entry}"
        )


        return jsonify({

            "status":
                "success",

            "message":
                "Telemetry logged successfully",

            "data":
                data_entry

        }), 200


    except (ValueError, TypeError) as e:

        return jsonify({

            "status":
                "error",

            "message":
                f"Invalid telemetry data: {e}"

        }), 400


# =========================================================
# MAIN
# =========================================================

if __name__ == '__main__':

    app.run(
        host='0.0.0.0',
        port=5000,
        debug=True
    )