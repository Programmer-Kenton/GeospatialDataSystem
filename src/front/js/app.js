// 当前页数和每页显示的条数
let currentPage = 1;
const itemsPerPage = 10;

// 存储查询结果数据（全局变量）
let geoData = [];

// DOM元素事件监听
document.getElementById('queryForm').addEventListener('submit', handleQuery);
document.getElementById('downloadBtn').addEventListener('click', handleDownload);
document.getElementById('randomGenerateBtn').addEventListener('click', handleRandomGenerate);
document.getElementById('randomDeleteBtn').addEventListener('click', handleRandomDelete);
document.getElementById('totalEntriesBtn').addEventListener('click', handleTotalEntries);

//
// 查询地理数据：提交查询请求，解析 JSON 请求体，验证坐标格式，并调用后端查询接口
//
async function handleQuery(event) {
    event.preventDefault();

    // 从输入框获取坐标字符串
    const inputText = document.getElementById('coords').value.trim();

    try {
        // 将输入字符串解析为二维数组
        const coordinates = parseCoordinates(inputText);
        validateCoordinates(coordinates);

        // 向后端 /query 接口发送 POST 请求
        const response = await fetch('http://192.168.232.129:8080/query', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ coordinates }),
            mode: 'cors'
        });

        if (!response.ok) throw new Error(`HTTP错误: ${response.status}`);

        const data = await response.json();
        updateUIAfterQuery(data);
        await refreshDataCount();

    } catch (error) {
        console.error('查询失败:', error);
        showToast(`查询失败: ${error.message}`, "error");
    }
}

//
// 将输入字符串转换为二维数组
// 输入示例："75.692101,8.418863 -142.224468,70.396431 ..."
// 输出：[[75.692101,8.418863],[-142.224468,70.396431],...]
//
function parseCoordinates(input) {
    return input.split(" ").map(pair => {
        const [lng, lat] = pair.split(",");
        return [parseFloat(lng), parseFloat(lat)];
    }).filter(([lng, lat]) => !isNaN(lng) && !isNaN(lat));
}

//
// 验证坐标数据：至少需要 3 个点构成多边形
//
function validateCoordinates(coords) {
    if (coords.length < 3) throw new Error("面查询至少需要三个坐标点");
}

//
// 更新查询结果的 UI 显示，包括数据统计、分页、下载按钮等
//
function updateUIAfterQuery(data) {
    geoData = data.data || [];
    document.getElementById('pointCount').textContent = data.statistics?.point_count || 0;
    document.getElementById('lineCount').textContent = data.statistics?.line_count || 0;
    document.getElementById('polygonCount').textContent = data.statistics?.polygon_count || 0;
    currentPage = 1;
    displayData();
    document.getElementById('downloadBtn').style.display = 'inline-block';
}

//
// 分页显示数据，将 geoData 按每页 itemsPerPage 条数据展示到表格中
//
function displayData() {
    const tableBody = document.getElementById('geoDataBody');
    tableBody.innerHTML = '';

    const start = (currentPage - 1) * itemsPerPage;
    const pageData = geoData.slice(start, start + itemsPerPage);

    // 显示表头
    document.querySelector('#geoTable thead').hidden = false;

    // 填充表格，每一行显示 id、type 和格式化后的 coordinates
    pageData.forEach(item => {
        tableBody.innerHTML += `
            <tr>
                <td>${item.id}</td>
                <td>${item.type}</td>
                <td>${formatCoordinates(item.coordinates)}</td>
                <td><button onclick="confirmDelete('${item.id}')">删除</button></td>
            </tr>
        `;
    });

    updatePagination();
}

//
// 格式化坐标显示：修正后端返回的字符串格式
// 后端返回格式为 "[(min_x, min_y), (max_x, max_y)]"
// 本函数将其转换为 "min_x,min_y max_x,max_y"，并固定 6 位小数
//
function formatCoordinates(coords) {
    // 如果 coords 为空，返回 "无数据"
    if (!coords) {
        return "无数据";
    }

    // 如果传入的是字符串，假设格式为 "[(min_x, min_y), (max_x, max_y)]"
    if (typeof coords === "string") {
        // 去除所有方括号和圆括号
        let clean = coords.replace(/[\[\]\(\)]/g, "");
        // 结果类似 "min_x, min_y, max_x, max_y"
        let parts = clean.split(",").map(s => s.trim());
        // 至少应有 4 个部分
        if (parts.length < 4) {
            return "无效";
        }
        let min_x = parseFloat(parts[0]);
        let min_y = parseFloat(parts[1]);
        let max_x = parseFloat(parts[2]);
        let max_y = parseFloat(parts[3]);
        if (isNaN(min_x) || isNaN(min_y) || isNaN(max_x) || isNaN(max_y)) {
            return "无效";
        }
        return `${min_x.toFixed(6)},${min_y.toFixed(6)} ${max_x.toFixed(6)},${max_y.toFixed(6)}`;
    }

    // 如果已经是数组格式，则采用原有逻辑
    if (Array.isArray(coords)) {
        return Array.isArray(coords[0])
            ? coords.map(c => isNaN(c[0]) || isNaN(c[1]) ? "无效" : `${c[0].toFixed(6)},${c[1].toFixed(6)}`).join(" ")
            : `${coords[0].toFixed(6)},${coords[1].toFixed(6)}`;
    }

    return "无效";
}

//
// 更新分页控件，根据 geoData 的总数生成页码按钮
//
function updatePagination() {
    const pagination = document.getElementById('pagination');
    pagination.innerHTML = '';
    const totalPages = Math.ceil(geoData.length / itemsPerPage);
    const maxVisible = 5;

    let startPage = Math.max(1, currentPage - 2);
    let endPage = Math.min(totalPages, currentPage + 2);

    if (currentPage <= 3) {
        endPage = Math.min(5, totalPages);
    } else if (currentPage >= totalPages - 2) {
        startPage = Math.max(totalPages - 4, 1);
    }

    if (startPage > 1) addPageButton(1, 1);
    if (startPage > 2) addPageButton('...', -1, 'disabled');

    for (let i = startPage; i <= endPage; i++) {
        addPageButton(i, i);
    }

    if (endPage < totalPages - 1) addPageButton('...', -1, 'disabled');
    if (endPage < totalPages) addPageButton(totalPages, totalPages);
}

//
// 删除数据前确认
//
function confirmDelete(id) {
    if (confirm("确定要删除该数据吗？")) {
        handleDelete(id);
    }
}

//
// 删除数据：向后端发送 DELETE 请求，更新 geoData 和 UI
//
async function handleDelete(id) {
    const btn = document.querySelector(`button[onclick="confirmDelete('${id}')"]`);
    try {
        btn.disabled = true;
        btn.innerHTML = '<i class="fas fa-spinner fa-spin"></i> 删除中...';

        const response = await fetch(`http://192.168.232.129:8080/delete/${id}`, {
            method: 'DELETE',
            mode: 'cors'
        });

        if (!response.ok) throw new Error(`删除失败: ${response.status}`);

        geoData = geoData.filter(item => item.id !== id);
        adjustPagination();
        await refreshDataCount();
    } catch (error) {
        showToast(`删除失败: ${error.message}`, "error");
    } finally {
        btn.disabled = false;
        btn.innerHTML = '删除';
    }
}

//
// 调整分页：确保当前页号不超过总页数
//
function adjustPagination() {
    const totalPages = Math.ceil(geoData.length / itemsPerPage);
    if (currentPage > totalPages) currentPage = Math.max(1, totalPages);
    displayData();
}

//
// 获取总数据条目（请求 /count 接口），更新显示
//
async function handleTotalEntries() {
    try {
        const response = await fetch('http://192.168.232.129:8080/count');
        if (!response.ok) throw new Error(`HTTP错误: ${response.status}`);
        const data = await response.json();
        updateTotalEntriesDisplay(data.totalEntries);
        showToast("数据总量已更新", "success");
    } catch (error) {
        console.error('获取总数失败:', error);
        showToast("获取数据总数失败", "error");
    }
}

//
// 更新总数显示（动画效果）
//
function updateTotalEntriesDisplay(count) {
    const element = document.getElementById('totalEntries');
    element.textContent = `${count.toLocaleString()}`;
    element.classList.add('num-update');
    setTimeout(() => element.classList.remove('num-update'), 500);
}

//
// 自动刷新总数
//
async function refreshDataCount() {
    try {
        const response = await fetch('http://192.168.232.129:8080/count');
        const data = await response.json();
        updateTotalEntriesDisplay(data.totalEntries);
    } catch (error) {
        console.error('自动刷新总数失败:', error);
    }
}

//
// 随机生成数据：向后端 /insert 接口发送请求，生成随机条数数据
//
async function handleRandomGenerate() {
    const btn = document.getElementById('randomGenerateBtn');
    try {
        btn.disabled = true;
        btn.innerHTML = '<i class="fas fa-spinner fa-spin"></i> 生成中...';

        const randomNumber = Math.floor(Math.random() * 90000) + 10000;
        if (!confirm(`确认要生成 ${randomNumber.toLocaleString()} 条数据吗？`)) return;

        const response = await fetch('http://192.168.232.129:8080/insert', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ num: randomNumber }),
            mode: 'cors'
        });

        if (!response.ok) throw new Error(`生成失败: ${response.status}`);

        const data = await response.json();
        showToast(data.message || "数据生成成功", "success");
        await refreshDataCount();

    } catch (error) {
        console.error('生成失败:', error);
        showToast(`数据生成失败: ${error.message}`, "error");
    } finally {
        btn.disabled = false;
        btn.innerHTML = '<i class="fas fa-random"></i> 随机生成数据';
    }
}

//
// 随机删除数据：向后端 /delete-random 接口发送请求
//
async function handleRandomDelete() {
    const btn = document.getElementById('randomDeleteBtn');
    try {
        btn.disabled = true;
        btn.innerHTML = '<i class="fas fa-spinner fa-spin"></i> 删除中...';

        const num = prompt("请输入要删除的数据条数:");
        if (!num || num < 1) return;

        const response = await fetch('http://192.168.232.129:8080/delete-random', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ num: parseInt(num) }),
            mode: 'cors'
        });

        if (!response.ok) throw new Error(`删除失败: ${response.status}`);

        const data = await response.json();
        showToast(`成功删除 ${data.deleted_count} 条数据`, "success");
        await handleQuery(new Event('submit'));
        await refreshDataCount();

    } catch (error) {
        console.error('删除失败:', error);
        showToast(`删除失败: ${error.message}`, "error");
    } finally {
        btn.disabled = false;
        btn.innerHTML = '<i class="fas fa-trash"></i> 随机删除数据';
    }
}

//
// 下载 CSV 文件，将 geoData 数据拼接成 CSV 格式，并下载
//
function handleDownload() {
    const csvContent = [
        "ID,类型,坐标",
        ...geoData.map(item => `${item.id},${item.type},"${item.coordinates}"`)
    ].join("\n");

    const blob = new Blob([csvContent], { type: 'text/csv;charset=utf-8;' });
    const link = document.createElement('a');
    link.href = URL.createObjectURL(blob);
    link.download = `geo_data_${new Date().toISOString().slice(0, 10)}.csv`;
    link.click();
}

//
// Toast 提示组件，用于显示提示信息
//
function showToast(message, type = 'info') {
    const toast = document.createElement('div');
    toast.className = `toast ${type}`;
    toast.textContent = message;
    document.body.appendChild(toast);
    setTimeout(() => toast.remove(), 3000);
}


function addPageButton(label, pageNumber, type = "") {
    const button = document.createElement('button');
    button.className = `page-button ${pageNumber === currentPage ? 'active' : ''} ${type}`;
    button.textContent = label;
    button.dataset.page = pageNumber;
    if (pageNumber === -1) button.disabled = true;
    document.getElementById('pagination').appendChild(button);
}

// 事件委托绑定
document.getElementById('pagination').addEventListener('click', function(e) {
    if (e.target.classList.contains('page-button') && e.target.dataset.page > 0) {
        currentPage = parseInt(e.target.dataset.page);
        displayData();
    }
});