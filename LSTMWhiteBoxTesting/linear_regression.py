import numpy as np
from sklearn.linear_model import LinearRegression

# Open the input file and read the data
with open('input_file.txt', 'r') as f:
    data = f.read()

# Extract the WGI values
wgi_values = []
for line in data.split('\n'):
    if line.startswith('.Stacked.NPLSTM    WGI'):
        parts = line.split(':')
        wgi_values.append([float(x) for x in parts[1].split('|')[1:]])

# Check if there are any WGI values
if not wgi_values:
    print("No WGI values found in the input data.")
else:
    # Convert to numpy arrays
    X = np.array([i for i in range(len(wgi_values))])
    y = np.array([wgi[1] for wgi in wgi_values])

    # Reshape X for LinearRegression
    X = X.reshape(-1, 1)

    model = LinearRegression()
    model.fit(X, y)

    # Print the weight (coefficient) and intercept
    print(f"Weight (coefficient): {model.coef_[0]}")
    print(f"Intercept: {model.intercept_}")