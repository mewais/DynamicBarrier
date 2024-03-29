import subprocess
import csv
import plotly.graph_objs as go
import plotly.subplots as ps
import plotly.io as pio

def RunSpeedTest(program, threads, iterations):
    print(f"Running test {program} with {threads} threads and {iterations} iterations")
    command = f"/usr/bin/time -f '%e' ../build/{program} {threads} {iterations}"
    output = subprocess.check_output(command, shell=True, stderr=subprocess.STDOUT)
    output = output.decode("utf-8")
    return float(output)

def RunAllTests(programs, threads, iterations):
    results = {program: [] for program in programs}
    for program in programs:
        for t in threads:
            for i in iterations:
                time_taken = RunSpeedTest(program, t, i)
                results[program].append((t, i, time_taken))
    return results

def WriteCSV(results):
    print("Writing CSV Data")
    with open("Speed.csv", "w", newline="") as csvfile:
        fieldnames = ["Program", "Threads", "Iterations", "Execution Time (seconds)"]
        writer = csv.DictWriter(csvfile, fieldnames=fieldnames)

        writer.writeheader()
        for program, data in results.items():
            for item in data:
                writer.writerow({"Program": program, "Threads": item[0], "Iterations": item[1], "Execution Time (seconds)": item[2]})

def ReadCSV():
    results = {}
    with open("Speed.csv", "r", newline="") as csvfile:
        reader = csv.DictReader(csvfile)
        for row in reader:
            program = row["Program"]
            threads = int(row["Threads"])
            iterations = int(row["Iterations"])
            time_taken = float(row["Execution Time (seconds)"])
            if program not in results:
                results[program] = []
            results[program].append((threads, iterations, time_taken))
    return results

def PlotResults(results):
    print("Plotting Results")
    unique_threads = sorted(list(set([item[0] for sublist in results.values() for item in sublist])))
    fig = ps.make_subplots(rows=len(unique_threads), cols=1, subplot_titles=[f"Threads: {threads}" for threads in unique_threads])

    for i, threads in enumerate(unique_threads):
        traces = []
        for program, data in results.items():
            x_values = [item[1] for item in data if item[0] == threads]
            y_values = [item[2] for item in data if item[0] == threads]
            trace = go.Scatter(
                x=x_values,
                y=y_values,
                mode="lines+markers",
                name=program,
                legendgroup=threads
            )
            traces.append(trace)
        for trace in traces:
            fig.add_trace(trace, row=i+1, col=1)
        fig.update_xaxes(title_text="Number of Iterations", type="log", row=i+1, col=1)
        fig.update_yaxes(title_text="Execution Time (seconds)", row=i+1, col=1)

    # Set the image width and height
    fig.update_layout(width=1920, height=1080*len(unique_threads), showlegend=True)
    pio.write_image(fig, "Speed.png", height=1080, width=1920)

if __name__ == "__main__":
    programs = ["FlatBarrier", "TreeBarrier"]
    threads = [2**i for i in range(4, 7)]  # Powers of 2 from 2 to 64
    iterations_cycle = [i for i in range(1, 10)]
    iterations = []
    for i in range(3, 7):
        for j in iterations_cycle:
            iterations.append(j * 10 ** i)
    iterations.append(iterations_cycle[0] * 10 ** 7)
    
    results = RunAllTests(programs, threads, iterations)
    WriteCSV(results)
    PlotResults(results)
