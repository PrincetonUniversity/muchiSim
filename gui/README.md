
# Use for the first time (install)

        cd gui
        python3 -m venv env_gui
        source env_gui/bin/activate
        pip install --upgrade pip
        pip install PyQt5
        pip install matplotlib
        pip install imageio
        cd ..
        python3 gui/gui.py


# Use subsequent times (when opening a new terminal)

        cd gui; source env_gui/bin/activate; cd ..
        python3 gui/gui.py
