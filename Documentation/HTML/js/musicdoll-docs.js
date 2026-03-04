/**
 * MusicDoll Documentation Interactive Scripts
 */

document.addEventListener('DOMContentLoaded', function() {
    initializeNavigation();
    initializeTabs();
    initializeCodeBlocks();
    initializeSmoothScroll();
});

/**
 * Initialize sidebar navigation
 */
function initializeNavigation() {
    const navLinks = document.querySelectorAll('.doc-nav-link');
    
    navLinks.forEach(link => {
        link.addEventListener('click', function(e) {
            // Remove active class from all links
            navLinks.forEach(l => l.classList.remove('active'));
            
            // Add active class to clicked link
            this.classList.add('active');
            
            // Expand parent items
            let parent = this.parentElement;
            while (parent && !parent.classList.contains('doc-nav')) {
                if (parent.classList.contains('doc-nav-item')) {
                    parent.classList.add('expanded');
                }
                parent = parent.parentElement;
            }
        });
    });
    
    // Auto-expand sections with active links
    const activeLinks = document.querySelectorAll('.doc-nav-link.active');
    activeLinks.forEach(link => {
        let parent = link.parentElement;
        while (parent && !parent.classList.contains('doc-nav')) {
            if (parent.classList.contains('doc-nav-item')) {
                parent.classList.add('expanded');
            }
            parent = parent.parentElement;
        }
    });
}

/**
 * Initialize tab navigation
 */
function initializeTabs() {
    const tabContainers = document.querySelectorAll('.tabs-container');
    
    tabContainers.forEach(container => {
        const buttons = container.querySelectorAll('.tab-button');
        const contents = container.parentElement.querySelectorAll('.tab-content');
        
        buttons.forEach(button => {
            button.addEventListener('click', function() {
                const tabId = this.getAttribute('data-tab');
                
                // Deactivate all tabs in this group
                buttons.forEach(b => b.classList.remove('active'));
                contents.forEach(c => c.classList.remove('active'));
                
                // Activate selected tab
                this.classList.add('active');
                const targetContent = document.getElementById(tabId);
                if (targetContent) {
                    targetContent.classList.add('active');
                }
            });
        });
    });
}

/**
 * Add copy buttons to code blocks
 */
function initializeCodeBlocks() {
    const codeBlocks = document.querySelectorAll('pre');
    
    codeBlocks.forEach(block => {
        const button = document.createElement('button');
        button.className = 'ui-button copy-code-btn';
        button.textContent = '复制代码';
        button.style.cssText = 'position: absolute; right: 10px; top: 10px; padding: 4px 10px; font-size: 12px;';
        
        block.style.position = 'relative';
        block.appendChild(button);
        
        button.addEventListener('click', function() {
            const code = block.querySelector('code');
            const text = code ? code.textContent : block.textContent;
            
            navigator.clipboard.writeText(text).then(() => {
                button.textContent = '已复制!';
                setTimeout(() => {
                    button.textContent = '复制代码';
                }, 2000);
            }).catch(err => {
                console.error('复制失败:', err);
                button.textContent = '复制失败';
            });
        });
    });
}

/**
 * Initialize smooth scrolling for anchor links
 */
function initializeSmoothScroll() {
    const anchorLinks = document.querySelectorAll('a[href^="#"]');
    
    anchorLinks.forEach(link => {
        link.addEventListener('click', function(e) {
            const targetId = this.getAttribute('href').substring(1);
            const targetElement = document.getElementById(targetId);
            
            if (targetElement) {
                e.preventDefault();
                targetElement.scrollIntoView({
                    behavior: 'smooth',
                    block: 'start'
                });
                
                // Update URL without scrolling
                history.pushState(null, null, `#${targetId}`);
            }
        });
    });
}

/**
 * Highlight current section in navigation based on scroll position
 */
window.addEventListener('scroll', function() {
    const sections = document.querySelectorAll('section[id], h1[id], h2[id]');
    const navLinks = document.querySelectorAll('.doc-nav-link');
    
    let currentSection = '';
    
    sections.forEach(section => {
        const sectionTop = section.offsetTop;
        const sectionHeight = section.offsetHeight;
        
        if (window.pageYOffset >= sectionTop - 100) {
            currentSection = section.getAttribute('id');
        }
    });
    
    navLinks.forEach(link => {
        link.classList.remove('active');
        
        if (link.getAttribute('href') === `#${currentSection}`) {
            link.classList.add('active');
        }
    });
});

/**
 * Utility: Toggle dark/light theme (future enhancement)
 */
function toggleTheme() {
    document.body.classList.toggle('light-theme');
    localStorage.setItem('theme', document.body.classList.contains('light-theme') ? 'light' : 'dark');
}

/**
 * Utility: Expand/collapse all details elements
 */
function toggleAllDetails(expand = true) {
    const details = document.querySelectorAll('details');
    details.forEach(detail => {
        if (expand) {
            detail.setAttribute('open', '');
        } else {
            detail.removeAttribute('open');
        }
    });
}
