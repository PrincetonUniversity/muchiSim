This is the folder in which the temporal files are generated.
Temporal files allow generating the plots faster as it does not need to parse the entire trace file again.
However, if the temporal file exists, the parse script will not check the original file.
So if there are new updates to a file, change the override_temp flag inside parse.py to True, or remove the log files inside this temporal folder.