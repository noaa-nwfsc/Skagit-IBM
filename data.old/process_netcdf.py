
import netCDF4 as nc
import matplotlib.pyplot as plt
import numpy as np

map_fname = "data/map_geometry.csv"
with open(map_fname, mode='r') as map_file:
    map_nodes_dict = {}
    for line in map_file.readlines()[1:]:
        chunks = line.split(',')
        if len(chunks) != 3:
            continue
        map_nodes_dict[int(chunks[2])-1] = (float(chunks[0]), float(chunks[1]))

map_v_fname = "data/map_vertices.csv"
with open(map_v_fname, mode='r') as map_v_file:
    for line in map_v_file.readlines()[1:]:
        chunks = line.split(',')
        hab_type = chunks[6]
        node_id = int(chunks[0]) - 1
        if hab_type != 'distributary channel':
            del map_nodes_dict[node_id]

hydro_data_root = '/Volumes/Dell Portable Hard Drive/SSM_2014_Hydrodynamic_Solution_TK/3D_Currents-Salinity-Temp'

def get_hydro_file_name(i):
    i_s = str(i)
    padded = '0'*(5-len(i_s)) + i_s
    return hydro_data_root+'/3DCurrents_ssm_'+padded+'.nc'


hydro_data = nc.Dataset(get_hydro_file_name(1))

#map_points = np.array(list(map_nodes_dict.values()))
#min_coords = np.min(map_points, axis=0)
#max_coords = np.max(map_points, axis=0)
#plt.figure()
#plt.scatter(hydro_data.variables['X'][:], hydro_data.variables['Y'][:], marker=',', s=0.2, c='r')
#plt.scatter(map_points[:, 0], map_points[:, 1], marker=",", s=0.2)
#plt.xlim(min_coords[0], max_coords[0])
#plt.ylim(min_coords[1], max_coords[1])
#plt.show()

# filter irrelevant hydro nodes
relevant_ids = np.zeros(hydro_data.dimensions['node'].size, dtype=np.bool)
for node_x, node_y in map_nodes_dict.values():
    relevant_ids[np.argmin(np.sqrt(np.square(hydro_data['X'][:] - node_x) + np.square(hydro_data['Y'][:] - node_y)))] = True

hydro_condensed = nc.Dataset('data/flow_condensed.nc', 'w', format='NETCDF4')
hydro_condensed.createDimension('time')
hydro_condensed.createDimension('node', sum(relevant_ids))
x = hydro_condensed.createVariable('x', 'f8', ('node',))
y = hydro_condensed.createVariable('y', 'f8', ('node',))
u = hydro_condensed.createVariable('u', 'f4', ('time', 'node'))
v = hydro_condensed.createVariable('v', 'f4', ('time', 'node'))
#temp = hydro_condensed.createVariable('temp', 'f4', ('time', 'node'))
#wse = hydro_condensed.createVariable('wse', 'f4', ('time', 'node'))
#time = hydro_condensed.createVariable('time', 'f4', ('time',))

x[:] = hydro_data['X'][relevant_ids]
y[:] = hydro_data['Y'][relevant_ids]
u_slices = [np.average(hydro_data['u'][:, :, relevant_ids], axis=1)]
v_slices = [np.average(hydro_data['v'][:, :, relevant_ids], axis=1)]
#temp_slices = [np.average(hydro_data['temp'][:, :, relevant_ids], axis=1)]
#wse_slices = [hydro_data['zeta'][:, relevant_ids]]
#time_slices = [hydro_data['time'][:]]

hydro_data.close()

for i in range(2, 391):
    print('processing', get_hydro_file_name(i))
    hydro_data = nc.Dataset(get_hydro_file_name(i))

    u_slices.append(np.average(hydro_data['u'][:, :, relevant_ids], axis=1))
    v_slices.append(np.average(hydro_data['v'][:, :, relevant_ids], axis=1))
    #if 'temp' in hydro_data.variables.keys():
    #    temp_slices.append(np.average(hydro_data['temp'][:, :, relevant_ids], axis=1))
    #    wse_slices.append(hydro_data['zeta'][:, relevant_ids])
    #    time_slices.append(hydro_data['time'][:])

    hydro_data.close()

u[:] = np.concatenate(u_slices, axis=0)
v[:] = np.concatenate(v_slices, axis=0)
#temp[:] = np.concatenate(temp_slices, axis=0)
#wse[:] = np.concatenate(wse_slices, axis=0)
#time[:] = np.concatenate(time_slices, axis=0)

hydro_condensed.close()
