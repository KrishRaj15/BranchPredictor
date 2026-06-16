import streamlit as st
import subprocess
import json
import plotly.express as px
import pandas as pd
import os

st.set_page_config(page_title="Branch Predictor Lab", layout="wide")

st.title(" Branch Predictor Multi-Visualizer")
st.markdown("Comparing hardware prediction strategies and internal state transitions.")

# Sidebar Configuration
st.sidebar.header("Execution Settings")
binary_path = st.sidebar.text_input("C++ Binary Path", "./2")
trace_path = st.sidebar.text_input("Trace File Path", "branch_trace.txt")

if st.sidebar.button("Analyze Trace", type="primary"):
    if not os.path.exists(binary_path):
        st.error(f"Binary not found at {binary_path}")
    elif not os.path.exists(trace_path):
        st.error(f"Trace file not found at {trace_path}")
    else:
        
        try:
            result = subprocess.run([binary_path, trace_path], capture_output=True, text=True)
            
            if result.returncode != 0:
                st.error("C++ Program Error")
                st.code(result.stderr)
            else:
                data = json.loads(result.stdout)
                metrics = data["metrics"]
                
                # --- TABBED INTERFACE ---
                tab1, tab2, tab3 = st.tabs(["🏆 Accuracy Leaderboard", "📊 State Distribution", "🔍 Address Inspector"])

                with tab1:
                    st.subheader("Predictor Comparison")
                    
                    # Prepare data for comparison chart
                    # Filter out 'Total Branches' for the accuracy chart
                    acc_data = {k: v for k, v in metrics.items() if "Total" not in k}
                    df_acc = pd.DataFrame(list(acc_data.items()), columns=['Predictor', 'Accuracy (%)'])
                    df_acc = df_acc.sort_values(by='Accuracy (%)', ascending=False)

                    # Metric row for top performers
                    top_cols = st.columns(3)
                    top_cols[0].metric("Total Branches", f"{int(metrics['Total Branches']):,}")
                    top_cols[1].metric("Best Predictor", df_acc.iloc[0]['Predictor'])
                    top_cols[2].metric("Highest Accuracy", f"{df_acc.iloc[0]['Accuracy (%)']:.2f}%")

                    # Comparison Bar Chart
                    fig_comp = px.bar(
                        df_acc, x='Predictor', y='Accuracy (%)',
                        color='Accuracy (%)',
                        color_continuous_scale='Viridis',
                        text_auto='.2f'
                    )
                    fig_comp.update_layout(yaxis_range=[0, 105])
                    st.plotly_chart(fig_comp, width='stretch')

                with tab2:
                    st.subheader("Internal Predictor States")
                    btb_list = data.get("btb_state", [])
                    
                    if btb_list:
                        df_btb = pd.DataFrame(btb_list)
                        
                        # State Distribution Logic
                        state_counts = df_btb['state'].value_counts().reset_index()
                        state_counts.columns = ['State', 'Count']
                        
                        # Mapping for 2-bit states
                        labels = {0: "Strongly NT", 1: "Weakly NT", 2: "Weakly T", 3: "Strongly T"}
                        state_counts['Description'] = state_counts['State'].map(labels)
                        
                        fig_state = px.pie(
                            state_counts, values='Count', names='Description',
                            color='State',
                            color_discrete_map={0: "#ff4b4b", 1: "#ffa4a4", 2: "#a4ffa4", 3: "#00cc44"},
                            hole=0.4
                        )
                        st.plotly_chart(fig_state, width='stretch')
                        st.info(f"The Branch Target Buffer (BTB) is currently tracking **{data['btb_total_entries']}** unique branch addresses.")
                    else:
                        st.warning("No BTB state data available.")

                with tab3:
                    st.subheader("Address-Level State Table")
                    if btb_list:
                        df_btb = pd.DataFrame(btb_list)
                        # Add descriptive column
                        df_btb['Label'] = df_btb['state'].map(labels)
                        
                        # Search/Filter functionality
                        search = st.text_input("Filter by Hex Address (e.g., 0x40)")
                        if search:
                            df_btb = df_btb[df_btb['address'].str.contains(search)]
                        
                        st.dataframe(df_btb, width='stretch', height=500)
                    else:
                        st.write("Execute analysis to see address details.")

        except json.JSONDecodeError:
            st.error("The C++ output was not valid JSON. Check for debug print statements in your C++ code.")
            st.code(result.stdout)