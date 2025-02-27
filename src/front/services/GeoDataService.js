import axios from 'axios';

const apiUrl = 'http://localhost:3000/api'; // 后端 C++ 服务的 API 地址

export const queryGeoData = (coordinates) => {
    return axios.post(`${apiUrl}/query`, { coordinates }); // 向后端查询数据
};

export const downloadCSV = (geoData) => {
    return axios.post(`${apiUrl}/download`, { geoData }, { responseType: 'blob' });
};

export const deleteGeoData = (id) => {
    return axios.delete(`${apiUrl}/delete/${id}`);
};

export const randomGenerate = (number) => {
    return axios.post(`${apiUrl}/random-generate`, { number });
};
