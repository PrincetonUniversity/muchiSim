import sys,os

from PyQt5.QtWidgets import QApplication, QMainWindow, QVBoxLayout, QHBoxLayout, QWidget, QPushButton, QTextEdit, QLabel, QComboBox, QListWidget, QAbstractItemView, QFileDialog, QScrollArea, QLineEdit
from PyQt5.QtGui import QImage, QPixmap, QMovie
from PyQt5.QtCore import Qt
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.figure import Figure
import matplotlib.pyplot as plt
import numpy as np
import imageio.v2 as imageio

directory = "dlxsim/"
lista = []
matrix_perf_counters = ['FMCollision In','FMCollision Out','FMCollision End','FMTask1','FMTask2','FMTask3','FMCore Active','FMRouter Active']
average_perf_counters = ['Avg. Transactions','Avg. Latency']

statistic_options = ['Heatmap','Boxplot','Average', 'Maximum', 'Minimum', 'Standard Deviation']
apps = ['SSSP','PageRank',"BFS","WCC","SPMV","Histo","Multi"]
sizes = ['8','16','32','64','128','256','512','1024']

image_labels = []
movies = []

average_perf_counters = ['Avg. Transactions:', 'Avg. Latency:', 'DHit Rate (hit/miss):']
options = matrix_perf_counters + average_perf_counters

dataset = "Kron"

def parseAverage(configuration, application, size):
    path = directory + "DATA-"+ dataset + "--" + size + "-X-" + size + "--B" + configuration + "-A" + str(application) + ".log"
    with open(path, 'r') as f:
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

def parseMatrix(configuration, application, size):
    path = directory + "DATA-"+ dataset + "--" + size + "-X-" + size + "--B" + configuration + "-A" + str(application) + ".log"
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
                row = [float(x) for x in temp if x != '\n']
                matrix.append(row)
                matrix_size += 1

                # Check if the current matrix is complete
                if matrix_size == len(row):
                    database[current_delim].append(matrix)
                    matrix = []
                    found = False

    return database

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
        items = self.list_widget.findItems('FMCore Active', Qt.MatchExactly)
        self.list_widget.setCurrentItem(items[0])
        self.list_widget.setFixedHeight(100)  # Set fixed height
        self.main_layout.addWidget(self.list_widget)

        stats_layout = QHBoxLayout()
        stats_layout.addWidget(QLabel("Stats:", self))
        self.stats_dropdown = QComboBox()
        self.stats_dropdown.addItems(statistic_options)
        self.stats_dropdown.setCurrentText('Average')
        stats_layout.addWidget(self.stats_dropdown)
        self.main_layout.addLayout(stats_layout)

        # Dropdown for Size
        stats_layout = QHBoxLayout()
        stats_layout.addWidget(QLabel("Size:", self))
        self.size_dropdown = QComboBox()
        self.size_dropdown.addItems(sizes)
        self.size_dropdown.setCurrentText('8')
        stats_layout.addWidget(self.size_dropdown)
        self.main_layout.addLayout(stats_layout)

        # TextInput for configuration
        stats_layout = QHBoxLayout()
        stats_layout.addWidget(QLabel("Configuration:", self))

        self.config_input = QLineEdit(self)
        self.config_input.setPlaceholderText("Enter configuration here")

        stats_layout.addWidget(self.config_input)
        self.main_layout.addLayout(stats_layout)

        # Dropdown for application
        stats_layout = QHBoxLayout()
        stats_layout.addWidget(QLabel("Application:", self))
        self.app_dropdown = QComboBox()
        self.app_dropdown.addItems(apps)
        # Multi is 6, SPMV is 4
        self.app_dropdown.setCurrentIndex(4)
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


    def generate_image(self):
        # Replace this with your image generation code
        fig = Figure(figsize=(5, 4), dpi=100)
        ax = fig.add_subplot(111)
        ax.plot([1, 2, 3, 4], [1, 4, 2, 3])
        ax.set_title("My Plot")
        ax.set_xlabel("X-axis")
        ax.set_ylabel("Y-axis")
        
        canvas = FigureCanvas(fig)
        canvas.draw()

        width, height = fig.get_size_inches() * fig.get_dpi()
        image = canvas.buffer_rgba()
        qimage = QImage(image, int(width), int(height), QImage.Format_RGBA8888)
        pixmap = QPixmap.fromImage(qimage)
        
        return pixmap

    def save_image(self):
        # Open a file dialog to choose the save location and filename
        options = QFileDialog.Options()
        options |= QFileDialog.DontUseNativeDialog
        file_name, _ = QFileDialog.getSaveFileName(self, "Save Image", "", "Images (*.png);;All Files (*)", options=options)

        if file_name:  # Check if the user has chosen a file
            if not file_name.endswith('.png'):  # Add the .png extension if it's not present
                file_name += '.png'
            last = image_labels[-1]
            image = last.pixmap()  # Get the image from the label
            image.save(file_name)  # Save the image


    def generate_plot(self):
        selected_options = [item.text() for item in self.list_widget.selectedItems()]
        stat = self.stats_dropdown.currentText()
        config = self.config_input.text()
        size = self.size_dropdown.currentText()
        # app = self.app_dropdown.currentText()
        app_index = self.app_dropdown.currentIndex()
        data = parseMatrix(config, app_index,size)
        avg_data = parseAverage(config, app_index, size)

       
        for item in selected_options:
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

            if stat != "Heatmap":
                # Pass the selected options to your plotting function (use your existing code here)
                fig = Figure(figsize=(10, 6))
                ax = fig.add_subplot(111)

                ax.set_xlabel('Frame #', fontsize=12)
                ax.set_ylabel('Average', fontsize=12)
                ax.set_title('All Averages', fontsize=16)
                ax.grid()
                ax.plot(np.arange(len(statistics)), statistics, label=item)
                if stat == "Boxplot":
                    ax.boxplot([np.array(frame[1:]).flatten() for frame in frames])
                ax.legend(fontsize=16)
                # Convert the plot to a QImage and display it in the image view
                canvas = FigureCanvas(fig)
                canvas.draw()
                width, height = fig.get_size_inches() * fig.get_dpi()
                buf = canvas.buffer_rgba()
                qimage = QImage(buf, int(width), int(height), QImage.Format_RGBA8888)
                # QLabel for displaying the image
                new_image_label = QLabel()
                new_image_label.setPixmap(QPixmap.fromImage(qimage))
                self.image_layout.addWidget(new_image_label)
                image_labels.append(new_image_label)

            elif stat == "Heatmap":
                filenames = []
                for frame_number, frame in enumerate(data[item]):
                    plt.figure()
                    heatmap_data = np.array(frame[1:])
                    # Make the heatmap legend fixed from 0 to 100, where 0 is light blue and 100 is dark blue
                    plt.imshow(heatmap_data, cmap='Blues', interpolation='nearest', vmin=0, vmax=100)
                    plt.title(f"Heatmap for {item} - Frame {frame_number}")
                    plt.colorbar()

                    # Save each heatmap as a PNG file
                    filename = f'heatmap_{item}_frame_{frame_number}.png'
                    plt.savefig(filename)
                    filenames.append(filename)
                    plt.close()
                    print(f"Saved heatmap for {item} - Frame {frame_number} as {filename}")
                # Create a GIF from the saved heatmaps
                if filenames:
                    images = [imageio.imread(filename) for filename in filenames]
                    gif_path = f'heatmap_animation.gif'
                    imageio.mimsave(gif_path, images, duration=1)
                    print("GIF created successfully!")

                    movie = QMovie(gif_path)
                    new_movie_label = QLabel()
                    new_movie_label.setMovie(movie)
                    self.image_layout.addWidget(new_movie_label)
                    movies.append(new_movie_label)
                    movie.start()

                    # Remove temporary PNG files
                    # for filename in filenames:
                    #     os.remove(filename)

if __name__ == '__main__':
    app = QApplication(sys.argv)
    main_window = MainWindow()
    main_window.show()
    sys.exit(app.exec_())