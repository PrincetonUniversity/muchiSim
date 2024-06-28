import sys,os,io,argparse
try:
    from PyQt5.QtWidgets import QApplication, QMainWindow, QVBoxLayout, QHBoxLayout, QWidget, QPushButton, QTextEdit, QLabel, QComboBox, QListWidget, QAbstractItemView, QFileDialog, QScrollArea, QLineEdit, QMessageBox
    from PyQt5.QtGui import QImage, QPixmap, QMovie
    from PyQt5.QtCore import Qt
    from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg as FigureCanvas
    from matplotlib.figure import Figure
    import matplotlib.pyplot as plt
except ImportError:
    print("ERROR: Please install PyQt5 and matplotlib to use the GUI")
    exit()
import numpy as np
from datetime import datetime

if sys.version_info >= (3, 7):
    # print("Python 3.10 or newer detected.")
    python_3_10m = True
    import imageio.v2 as imageio
else:
    #print("Using an older version of Python.")
    python_3_10m = False
    import imageio

output_dir = "plots/"

lista = []

matrix_perf_counters = ['FMCollision In','FMCollision Out','FMCollision End','FMTask1','FMTask2','FMTask3','FMCore Active','FMRouter Active']
accum_perf_counters = ['AMTask1','AMTask2','AMTask3','AMCore Active','AMRouter Active','Avg. Transactions:']
average_perf_counters = ['Avg. Transactions:', 'Avg. Latency:', 'DHit Rate (hit/miss):']

options = matrix_perf_counters + average_perf_counters
matrix_perf_counters = matrix_perf_counters + accum_perf_counters

statistic_options = ['Heatmap','Boxplot','Average', 'Maximum', 'Minimum', 'Standard Deviation']
apps = ['SSSP','PageRank',"BFS","WCC","SPMV","Histo","FFT","SPMV Flex","SPMM","Multi"]
sizes = ['8','16','32','64','128','256','512','1024']
datasets = ['Kron16','Kron22','Kron25','Kron26','fft1','wikipedia','liveJournal']


# Create the parser
parser = argparse.ArgumentParser(description='Set parameters via command line.')
parser.add_argument('--nogui', action='store_true', help='Process directly without GUI')
parser.add_argument('-m', '--metric', default='FMCore Active', help='Metric name (default: %(default)s)')
parser.add_argument('-p', '--plot', default='Boxplot', help='Plot type (default: %(default)s)')
parser.add_argument('-s', '--size', default='64', help='Size parameter (default: %(default)s)')
parser.add_argument('-a', '--app', default=2, type=int, help='App number (default: %(default)i)')
parser.add_argument('-n', '--name', default='A16', help='Experiment name (default: %(default)s)')
parser.add_argument('-d', '--dataset', default='Kron22', help='Dataset name (default: %(default)s)')
parser.add_argument('-e', '--artifact_evaluation', default=0, type=int, help='Artifact evaluation number, changes sim logs folder (default: %(default)i)')
# Parse arguments
args = parser.parse_args()
default_metric = args.metric
default_plot = args.plot
default_size = args.size
default_app = args.app
default_name = args.name
default_dataset = args.dataset
nogui = args.nogui

if args.artifact_evaluation==1:
    log_dir = "sim_paper/"
else:
    log_dir = "sim_logs/"

if default_metric not in matrix_perf_counters:
    print(f"Metric {default_metric} not valid")
    exit()

print(f"Metric: {default_metric}, Plot: {default_plot}, Size: {default_size}, App: {default_app}, Name: {default_name}, Dataset: {default_dataset}")

image_labels = []
movies = []

def parseAverage(configuration, application, size, dataset):
    path = log_dir + "DATA-"+ dataset + "--" + size + "-X-" + size + "--B" + configuration + "-A" + str(application) + ".log"
    with open(path, 'r') as f:
        print(f"Reading file {path}")
        lines = f.readlines()
        # Create a list of lists, one for each counter
        database = {delim: [] for delim in average_perf_counters}
        for line in lines:
            for idx, counter in enumerate(average_perf_counters):
                if counter in line:
                    value = line.split(counter)[1]
                    # Avg. Latency does not have a '%' sign
                    if counter != 'Avg. Latency:':
                        value = value.split('%')[0].strip()
                    database[counter].append(float(value))
        return database

def parseMatrix(configuration, application, size, dataset):
    path = log_dir + "DATA-"+ dataset + "--" + size + "-X-" + size + "--B" + configuration + "-A" + str(application) + ".log"
    # Check if the file exists
    if not os.path.exists(path):
        print(f"File {path} does not exist")
        return None
    
    with open(path, 'r') as f:
        lines = f.readlines()
        database = {delim: [] for delim in matrix_perf_counters}
        current_delim = None
        matrix = []
        matrix_size = 0
        found = False

        for i, line in enumerate(lines):
            if any(delim in line for delim in matrix_perf_counters):
                current_delim = next((delim for delim in matrix_perf_counters if delim in line), None)
                found = True
                matrix_size = 0
            elif found:
                temp = line.split('\t')
                row = []
                for x in temp:
                    if x != '\n':
                        cleaned_value = x.replace(',', '')  # Remove commas
                        try:
                            row.append(float(cleaned_value))
                        except ValueError:
                            print(f"WARNING: Could not convert {cleaned_value} to float.")
                            row.append(0)
                            continue
                matrix.append(row)
                matrix_size += 1

                # Check if the current matrix is complete
                if matrix_size == len(row):
                    database[current_delim].append(matrix)
                    matrix = []
                    found = False
    return database

def generate_figure(config,data, avg_data, item, stat):
    statistics = []
    if item not in matrix_perf_counters:
        statistics = avg_data[item]
    else:
        frames = data[item]
        for frame in frames:
            matrix = np.array(frame[1:])
            if stat == "Average":
                statistics.append(np.mean(matrix))
            elif stat == "Maximum":
                statistics.append(np.max(matrix))
            elif stat == "Minimum":
                statistics.append(np.min(matrix))
            elif stat == "Standard Deviation":
                statistics.append(np.std(matrix))

    # Pass the selected options to your plotting function (use your existing code here)
    fig = Figure(figsize=(10, 6))
    ax = fig.add_subplot(111)
    ax.set_xlabel('Frame #', fontsize=12)
    # ax.set_ylabel('Average', fontsize=12)
    ax.set_title(config, fontsize=16)
    ax.grid()
    ax.plot(np.arange(len(statistics)), statistics, label=item)
    if stat == "Boxplot":
        ax.boxplot([np.array(frame[1:]).flatten() for frame in frames])
    ax.legend(fontsize=16)

    return fig

def create_heatmap(data, item, config, app_index, size, dataset):
    # Create a folder with the current timestamp
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    item_name = item.replace("FM", "")
    item_name = item_name.replace("Core", "PU")
    dirname = output_dir + "animated_heatmaps/" + timestamp + "_" + config + "_" + str(app_index) + "_" + size + "_" + dataset + "_" + item_name.replace(" ", "_")
    os.makedirs(dirname, exist_ok=True)

    filenames = []
    for frame_number, frame in enumerate(data[item]):
        plt.figure()
        heatmap_data = np.array(frame[1:])
        plt.imshow(heatmap_data, cmap='Blues', interpolation='nearest', vmin=0, vmax=100)
        plt.title(f"Heatmap for {item_name} - Frame {frame_number}")
        plt.colorbar()
        plt.subplots_adjust(left=0.05, right=0.95, top=0.95, bottom=0.05)
        # Save each heatmap in the created folder
        filename = f'{dirname}/heatmap_{item_name.replace(" ", "_")}_frame_{frame_number}.png'
        plt.savefig(filename, bbox_inches='tight', pad_inches=0.1)
        filenames.append(filename)
        plt.close()
        print(f"Saved heatmap for {item_name} - Frame {frame_number} as {filename}")

    # Create a GIF from the saved heatmaps
    if filenames:
        images = [imageio.imread(filename) for filename in filenames]
        gif_path = f'{dirname}/heatmap_animation.gif'
        imageio.mimsave(gif_path, images, duration=1)
        print("GIF created successfully!")
        return gif_path

    # Uncomment this part if you want to remove the temporary PNG files
    # for filename in filenames:
    #     os.remove(filename)


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("My Plot")
        self.setGeometry(100, 100, 800, 600)

        central_widget = QWidget(self)
        self.main_layout = QVBoxLayout(central_widget)
        self.setCentralWidget(central_widget)

        # QListWidget for multiple selection
        self.list_widget = QListWidget()
        self.list_widget.setSelectionMode(QAbstractItemView.ExtendedSelection)
        self.list_widget.addItems(options)
        items = self.list_widget.findItems(default_metric, Qt.MatchExactly)
        self.list_widget.setCurrentItem(items[0])
        self.list_widget.setFixedHeight(100)  # Set fixed height
        self.main_layout.addWidget(self.list_widget)

        stats_layout = QHBoxLayout()
        stats_layout.addWidget(QLabel("Stats:", self))
        self.stats_dropdown = QComboBox()
        self.stats_dropdown.addItems(statistic_options)
        self.stats_dropdown.setCurrentText(default_plot)
        stats_layout.addWidget(self.stats_dropdown)
        self.main_layout.addLayout(stats_layout)

        # Dropdown for Size
        stats_layout = QHBoxLayout()
        stats_layout.addWidget(QLabel("Size:", self))
        self.size_dropdown = QComboBox()
        self.size_dropdown.addItems(sizes)
        self.size_dropdown.setCurrentText(default_size)
        stats_layout.addWidget(self.size_dropdown)
        self.main_layout.addLayout(stats_layout)

        # TextInput for configuration
        stats_layout = QHBoxLayout()
        stats_layout.addWidget(QLabel("Configuration:", self))
        self.config_input = QLineEdit(self)
        self.config_input.setPlaceholderText("Enter configuration here")
        self.config_input.setText(default_name)
        stats_layout.addWidget(self.config_input)
        self.main_layout.addLayout(stats_layout)

        # Dropdown for dataset
        stats_layout = QHBoxLayout()
        stats_layout.addWidget(QLabel("Dataset:", self))
        self.dataset_dropdown = QComboBox()
        self.dataset_dropdown.addItems(datasets)
        self.dataset_dropdown.setCurrentText(default_dataset)
        stats_layout.addWidget(self.dataset_dropdown)
        self.main_layout.addLayout(stats_layout)

        # Dropdown for application
        stats_layout = QHBoxLayout()
        stats_layout.addWidget(QLabel("Application:", self))
        self.app_dropdown = QComboBox()
        self.app_dropdown.addItems(apps)
        # Multi is 6, SPMV is 4
        self.app_dropdown.setCurrentIndex(default_app)
        stats_layout.addWidget(self.app_dropdown)
        self.main_layout.addLayout(stats_layout)

         # Create a scroll area
        self.scroll_area = QScrollArea()
        self.scroll_area.setWidgetResizable(True)
        self.main_layout.addWidget(self.scroll_area)
        # Create a widget to hold the images
        self.image_container = QWidget()
        self.scroll_area.setWidget(self.image_container)
        # Create a layout for the image_container
        self.image_layout = QVBoxLayout(self.image_container)

        # Add a button to generate the plot
        generate_plot_button = QPushButton("Generate Plot")
        generate_plot_button.clicked.connect(self.generate_plot)
        self.main_layout.addWidget(generate_plot_button)

        # Save Image button
        save_button = QPushButton("Save Image")
        self.main_layout.addWidget(save_button)
        save_button.clicked.connect(self.save_image)

    def save_image(self):
        # Open a file dialog to choose the save location and filename
        options = QFileDialog.Options()
        options |= QFileDialog.DontUseNativeDialog
        file_name, _ = QFileDialog.getSaveFileName(self, "Save Image", "", "Images (*.png);;All Files (*)", options=options)

        if file_name:  # Check if the user has chosen a file
            if not file_name.endswith('.png'):
                file_name += '.png'
            last = image_labels[-1]
            image = last.pixmap()  # Get the image from the label
            image.save(file_name)  # Save the image

    def show_warning(self, message):
        msg = QMessageBox()
        msg.setIcon(QMessageBox.Warning)
        msg.setText(message)
        msg.setWindowTitle("Warning")
        msg.exec_()
        
    def generate_plot(self):
        selected_options = [item.text() for item in self.list_widget.selectedItems()]
        stat = self.stats_dropdown.currentText()
        config = self.config_input.text()
        size = self.size_dropdown.currentText()
        app_index = self.app_dropdown.currentIndex()
        dataset = self.dataset_dropdown.currentText()

        data = parseMatrix(config, app_index,size,dataset)
        if data is None:
            print("The simulation log file does not exist")
            self.show_warning("The simulation log file does not exist")
            return
        avg_data = parseAverage(config, app_index, size, dataset)

        for item in selected_options:
            if stat != "Heatmap":
                fig = generate_figure(config, data, avg_data, item, stat)
                # Convert the plot to a QImage and display it in the image view
                canvas = FigureCanvas(fig); canvas.draw()
                width, height = fig.get_size_inches() * fig.get_dpi()
                buf = canvas.buffer_rgba()
                qimage = QImage(buf, int(width), int(height), QImage.Format_RGBA8888)
                # QLabel for displaying the image
                new_image_label = QLabel()
                new_image_label.setPixmap(QPixmap.fromImage(qimage))
                self.image_layout.addWidget(new_image_label)
                image_labels.append(new_image_label)

            elif stat == "Heatmap":
                gif_path = create_heatmap(data, item, config, app_index, size, dataset)
                movie = QMovie(gif_path)
                new_movie_label = QLabel()
                new_movie_label.setMovie(movie)
                self.image_layout.addWidget(new_movie_label)
                movies.append(new_movie_label)
                movie.start()


if __name__ == '__main__':
    if not nogui:
        app = QApplication(sys.argv)
        main_window = MainWindow()
        main_window.show()
        sys.exit(app.exec_())
    else:
        if default_name == "":
            print("Please provide a configuration name with the -n flag")
            exit()
        data = parseMatrix(default_name, default_app,default_size,default_dataset)
        if data is None:
            print("The simulation log file does not exist")
            exit(1)
        avg_data = parseAverage(default_name, default_app, default_size, default_dataset)

        if default_plot != "Heatmap":
            fig = generate_figure(default_name, data, avg_data, default_metric, default_plot)
            buf = io.BytesIO()
            fig.savefig(buf, format='png')
            # Go to the beginning of the BytesIO buffer
            buf.seek(0)
            # Create a QImage from the buffer
            qimage = QImage()
            qimage.loadFromData(buf.getvalue(), 'PNG')
            current_dir = os.getcwd() + "/"+ output_dir + "images"
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            item_name = default_metric.replace("FM", "").replace("Core", "PU")
            filename = timestamp + "_" + default_name + "_" + str(default_app) + "_" + default_size + "_" + default_dataset + "_" + item_name.replace(" ", "_")
            full_path = os.path.join(current_dir, filename + ".png")
            qimage.save(full_path)
            
        elif default_plot == "Heatmap":
            gif_path = create_heatmap(data, default_metric, default_name, default_app, default_size, default_dataset)