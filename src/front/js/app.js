let currentPage = 1;       // 当前页数
const itemsPerPage = 10;   // 每页显示的条数
let geoData = [];          // 存储查询结果数据

document.getElementById('queryForm').addEventListener('submit', handleQuery);
document.getElementById('downloadBtn').addEventListener('click', handleDownload);
document.getElementById('randomGenerateBtn').addEventListener('click', handleRandomGenerate);
document.getElementById('randomDeleteBtn').addEventListener('click', handleRandomDelete);


// 查询地理数据
function handleQuery(event) {
    event.preventDefault();

    let inputText = document.getElementById('coords').value.trim();
    // 将字符串转换成数组（例如："75.692101,8.418863 -142.224468,70.396431 ..."）
    let coordinates = inputText.split(" ").map(pair => {
        let [lng, lat] = pair.split(",");
        return [parseFloat(lng), parseFloat(lat)];
    });

    if (coordinates.length < 3) {
        alert("错误: 面查询至少需要三个坐标点！");
        return;
    }

    console.log('请求的URL:', 'http://192.168.232.129:8080/query');

    // 向后端发送请求获取数据
    fetch('http://192.168.232.129:8080/query', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({coordinates: coordinates}),
        mode: 'cors'
    })
        .then(response => {
            if (!response.ok) {
                throw new Error('查询失败: ' + response.statusText);
            }
            return response.json();
        })
        .then(data => {
            console.log('查询结果:', data);
            // 更新数据和统计信息
            geoData = data.data || [];
            document.getElementById('pointCount').innerText = data.statistics.point_count;
            document.getElementById('lineCount').innerText = data.statistics.line_count;
            document.getElementById('polygonCount').innerText = data.statistics.polygon_count;
            currentPage = 1; // 重置页码
            displayData();
        })
        .catch(error => {
            console.error('Error querying geo data:', error);
            alert('查询数据失败: ' + error.message);
        });
}

// 显示数据并分页
function displayData() {
    const tableBody = document.getElementById('geoDataBody');
    tableBody.innerHTML = '';

    const startIndex = (currentPage - 1) * itemsPerPage;
    const endIndex = startIndex + itemsPerPage;
    const pageData = geoData.slice(startIndex, endIndex);

    // 显示表头
    const tableHead = document.querySelector('#geoTable thead');
    tableHead.removeAttribute('hidden');

    // 填充表格数据
    pageData.forEach(item => {
        const row = document.createElement('tr');
        row.innerHTML = `
            <td>${item.id}</td>
            <td>${item.type}</td>
            <td>${item.coordinates}</td>
            <td>
                <button onclick="confirmDelete('${item.id}')">删除</button>
            </td>
        `;
        tableBody.appendChild(row);
    });

    // 显示表格和下载按钮
    document.getElementById('geoTable').style.display = 'table';
    document.getElementById('downloadBtn').style.display = 'inline-block';
    updatePagination();
}

// 更新分页按钮
function updatePagination() {
    const paginationDiv = document.getElementById('pagination');
    paginationDiv.innerHTML = '';

    const totalPages = Math.ceil(geoData.length / itemsPerPage);
    for (let i = 1; i <= totalPages; i++) {
        const pageButton = document.createElement('button');
        pageButton.textContent = i;
        pageButton.classList.add('page-button');
        if (i === currentPage) {
            pageButton.classList.add('active'); // 当前页按钮高亮
        }
        pageButton.onclick = () => goToPage(i);
        paginationDiv.appendChild(pageButton);
    }
}

// 跳转到指定页
function goToPage(page) {
    currentPage = page;
    displayData();
}


// 删除数据前提示确认
function confirmDelete(id) {
    if (confirm("确定要删除该数据吗？")) {
        handleDelete(id);
    }
}

// 删除数据
async function handleDelete(id) {
    try {
        // 显示加载状态
        const btn = document.querySelector(`button[onclick="confirmDelete('${id}')"]`);
        btn.disabled = true;
        btn.innerHTML = '<i class="fas fa-spinner"></i> 删除中...';

        // 发送请求
        const response = await fetch(`http://192.168.232.129:8080/delete/${encodeURIComponent(id)}`, {
            method: 'DELETE',
            headers: {
                'Content-Type': 'application/json',
                'Accept': 'application/json'
            },
            mode: 'cors'  // 确保 CORS 设置正确
        });

        // 调试输出请求响应
        console.log("Delete response status:", response.status);
        const responseText = await response.text();
        console.log("Delete response body:", responseText);

        // 处理响应
        if (!response.ok) {
            throw new Error(`HTTP错误: ${response.status}, ${responseText}`);
        }

        // 更新本地数据
        geoData = geoData.filter(item => item.id !== id);

        // 智能调整分页
        const totalPages = Math.ceil(geoData.length / itemsPerPage);
        if (currentPage > totalPages) currentPage = Math.max(1, totalPages);

        displayData();
    } catch (error) {
        console.error('删除失败:', error);
        alert(`删除失败: ${error.message}`);
    } finally {
        const btn = document.querySelector(`button[onclick="confirmDelete('${id}')"]`);
        if (btn) {
            btn.disabled = false;
            btn.innerHTML = '删除';
        }
    }
}

// 随机生成数据
function handleRandomGenerate() {

    const btn = document.getElementById('randomGenerateBtn');
    btn.disabled = true;
    btn.innerHTML = '<i class="fas fa-spinner"></i> 生成中...';

    // 生成一个1万到10万之间的随机数字
    const randomNumber = Math.floor(Math.random() * (100000 - 10000 + 1)) + 10000;

    // 提示用户生成的数字
    alert("随机插入的数据条数为: " + randomNumber);

    // 向后端发送请求
    fetch('http://192.168.232.129:8080/insert', {
        method: 'POST',
        mode: 'cors',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({num: randomNumber})  // 发送随机生成的数字
    })
        .then(response => {
            if (response.ok) {
                return response.json();  // 假设后端返回的是JSON数据
            } else {
                alert('数据生成失败');
                throw new Error('Failed to generate data');
            }
        })
        .then(data => {
            // 如果后端成功插入数据，可以根据返回的信息做进一步处理
            if (data.status === 'success') {
                alert('数据生成并提交成功');
            } else {
                alert('数据生成失败');
            }
        })
        .catch(error => {
            console.error('Error generating random data:', error);
            alert('随机生成数据请求错误');
        })
        .finally(() =>{
            btn.disabled = false;
            btn.innerHTML = '<i class="fas fa-random"></i> 随机生成数据';
        });
}


function handleRandomDelete() {

    const btn = document.getElementById('randomDeleteBtn');
    btn.disabled = true;
    btn.innerHTML = '<i class="fas fa-spinner"></i> 删除中...';

    const num = prompt("请输入要删除的数据条数: ");
    if (num === null || num < 1) {
        alert("请输入有效的删除数量");
        btn.disabled = false;
        btn.innerHTML = '<i class="fas fa-trash"></i> 随机删除数据';
        return;
    }

    fetch('http://192.168.232.129:8080/delete-random', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({num: parseInt(num)}),
        mode: 'cors'
    })
        .then(response => {
            if (!response.ok) {
                throw new Error('删除失败: ' + response.statusText);
            }
            return response.json();
        })
        .then(data => {
            if (data.status === 'success') {
                alert(`成功删除 ${data.deleted_count} 条数据`);
                // 刷新数据
                handleQuery(new Event('submit'));
            } else {
                alert('删除失败: ' + data.error);
            }
        })
        .catch(error => {
            console.error('Error deleting random data:', error);
            alert('删除数据失败: ' + error.message);
        })
        .finally(() =>{
            btn.disabled = false;
            btn.innerHTML = '<i class="fas fa-trash"></i> 随机删除数据';
        });
}


// 下载 CSV 文件
function handleDownload() {
    const csvContent = "ID,类型,坐标\n" + geoData.map(item => `${item.id},${item.type},${item.coordinates}`).join("\n");
    const blob = new Blob([csvContent], {type: 'text/csv'});
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = 'geo_data.csv';
    a.click();
}
