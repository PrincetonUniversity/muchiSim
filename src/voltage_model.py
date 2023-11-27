from sklearn.linear_model import LinearRegression, Ridge
import numpy as np


def model_voltage(transistor_nodes, frequencies_list, min_voltages_list):
    # Prepare the data for training the model
    X = []
    y = []
    for node, frequencies, min_voltages in zip(transistor_nodes, frequencies_list, min_voltages_list):
        for f, v in zip(frequencies, min_voltages):
            X.append([f, node])
            y.append(v)
    X = np.array(X)
    y = np.array(y)

    #Higher Alpha Value: A higher alpha would mean stronger regularization. This penalizes large coefficients and thus tends to reduce their size, driving them toward zero but not exactly to zero. The formula would thus be biased toward simpler models. This is particularly useful when you're dealing with overfitting.
    # Lower Alpha Value: A smaller alpha reduces the regularization term's effect. As alpha approaches zero, Ridge Regression tends to behave like a standard Linear Regression model. This might result in larger coefficients, especially if the model tries to fit the noise in the data.
    # Alpha Equals Zero: When alpha is zero, Ridge Regression is identical to Linear Regression, so you would get the same coefficients you would get if you were using ordinary least squares (OLS).
    model = Ridge(alpha=10)
    model.fit(X, y)

    # Obtain the coefficients and intercept
    coef = model.coef_
    intercept = model.intercept_
    print(f"The formula for the trained model is y = {intercept:0.2} + {coef[0]:0.2} * freq + {coef[1]:0.2} * transistor_node")

    # Function to predict minimal voltage for given frequency and transistor node
    def predict_min_voltage(frequency, node):
        # return model.predict(np.array([[frequency, node]]))[0]
        return intercept + coef[0] * frequency + coef[1] * node
    
    return predict_min_voltage

def test_cases():
    # Predict the minimum voltage required for a frequency
    target_frequency = 0.5  # in GHz
    target_node = 5  # in nanometers
    predicted_voltage = predict_min_voltage(target_frequency, target_node)
    print(f"Predicted minimum voltage required for {target_frequency} MHz at {target_node}nm node: {predicted_voltage:0.2} Volts")

    target_node = 7  # in nanometers
    predicted_voltage = predict_min_voltage(target_frequency, target_node)
    print(f"Predicted minimum voltage required for {target_frequency} MHz at {target_node}nm node: {predicted_voltage:0.2} Volts")

    target_node = 12
    predicted_voltage = predict_min_voltage(target_frequency, target_node)
    print(f"Predicted minimum voltage required for {target_frequency} MHz at {target_node}nm node: {predicted_voltage:0.2} Volts")

freq_5nm = np.array([0.5, 1.0, 1.5, 2.5,  2.7,  2.9, 3.1,  3.3, 3.5, 3.7, 3.9, 4.1, 4.3])  # in GHz
volt_5nm = np.array([0.4, 0.45, 0.6,  0.675, 0.7, 0.725, 0.75, 0.75, 0.775, 0.8, 0.825, 0.85, 0.9])  # in Volts

freq_7nm = np.array([0.5, 1.0, 1.5, 2.8, 3.0, 3.5, 4.0, 4.2])  # in GHz
volt_7nm = np.array([0.45, 0.5, 0.7, 0.825, 0.95, 1.2, 1.375])  # in Volts

freq_12nm = np.array([0.42, 0.58, 0.7, 0.78, 0.85, 0.95, 1.0, 1.05, 1.10, 1.15, 1.20, 1.22, 1.25])
volt_12nm = np.array([0.6, 0.65, 0.7, 0.75, 0.8, 0.85, 0.9, 0.95, 1.00, 1.05,  1.1, 1.15, 1.20])  # in Volts

transistor_nodes = [5, 7, 12]  # in nanometers
frequencies_list = [freq_5nm, freq_7nm, freq_12nm]
min_voltages_list = [volt_5nm, volt_7nm, volt_12nm]

predict_min_voltage = model_voltage(transistor_nodes, frequencies_list, min_voltages_list)

test = 0
if test:
    test_cases()
else:
    # Setup args with two parameters -f and -n
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("-f", "--frequency", help="Frequency in MHz", type=float, default=1.0)
    parser.add_argument("-n", "--node", help="Transistor node in nanometers", type=int, default=7)
    args = parser.parse_args()

    # Predict the minimum voltage required for a frequency
    target_frequency = args.frequency  # in GHz
    target_node = args.node  # in nanometers
    predicted_voltage = predict_min_voltage(target_frequency, target_node)
    print(f"Predicted minimum voltage required for {target_frequency} MHz at {target_node}nm node: {predicted_voltage:0.2}")
    print(f"{predicted_voltage:0.2}")